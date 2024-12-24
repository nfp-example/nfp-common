/*a Copyright */
/**
 Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/
/*a Documentation */
/**
 * @file        data_coproc_lib.c
 * @brief       A simple data coprocessor
 *
 * The data coprocessor firmware takes work from a number of host work
 * queues and despatches it to MEs that handle the work; the worker
 * MEs deliver results back when they complete, permitting the host
 * work queue to be updated with the results.
 *
 * The firmware work despatch architecture consists of:
 *
 * 1. Work queue manager thread, which monitors the cluster scratch
 * work queues (written by the host), and uses ME local memory to
 * cache these queue details, and a shared register to capture which
 * work queues are enabled.
 *
 * 2. Work gathering thread, which DMAs up to four work items from a
 * single host work queue in to memory at a time, based on the cached
 * work ``queue descriptors updated by the work queue manager thread;
 * after the DMA is requested individual work iterms are added to the
 * work queue to be delivered to the worker threads.
 *
 * 3. Worker threads; these add themselves to the real internal MU
 * work queue, and receive work from the work gathering thread. This
 * work consists of a host work queue number and an address in memory
 * where the work descriptor will be delivered by DMA (initiated by
 * the work gatherer). The work descriptor is guaranteed to be
 * non-zero, and so the worker thread should read the work descriptor
 * from the memory; if it reads zeros, it should retry (this is only
 * going to happen under light load, when work does not get backed
 * up). Once the work descriptor is in the worker thread, it must
 * clear the descriptor in memory (that it just read, ready for the
 * next time) and it performs the work - the chances are this means
 * DMA more data from the host (as described by the work queue entry),
 * operate on the data, and DMA results back to the host - certainly
 * it must involve DMAing a result back to the host work queue entry,
 * which will indicate the work has been completed (and the host will
 * then be able to use the results).
 *
 * There is a credit system used to stop the work gatherer putting too
 * much work into the MU work queue - this involves a global memory
 * location that the work gatherer monitors. This contains the number
 * of work items completed by the workers - i.e. it is incremented
 * when a worker thread completes work and adds itself back to the MU
 * work queue. The work gatherer is not permitted to add more than 'N'
 * items of work beyond the number of work items completed. The value
 * of 'N' should be no larger than the MU work queue, of course. If
 * the work gatherer is about to add item 'N+M' (where M is the number
 * of work items it believes have been completed) then it must poll
 * read M (the number of work items completed with a backoff until it
 * is permitted to add the item. The overhead of the credit scheme is
 * very low; the work gatherer will have to read the credits at most
 * every M items if the system is not overloaded, and at overload the
 * polling of reading M is only by a single thread with a backoff - a
 * low system penalty that will not worsen the overload condition.
 *
 *  @dot
 *  digraph example {
 *     graph [fontsize=10 fontname="Verdana" compound=true newrank=true];
 *     node [shape=record fontsize=10 fontname="Verdana"];
 * 
 *     subgraph cluster_host {
 *       label = "Host";
 *       color=blue;
 *       node [shape=doubleoctagon];
 *       host_server [label="Host server thread"];
 *
 *       node [shape=record];
 *
 *       host_memory [shape=box3d; label="Host memory"];
 *     }
 * 
 *     subgraph cluster_mu_work_data {
 *       label = "MU work data";
 *       color=blue;
 *
 *       mu_work_data [shape=box3d; label="MU work buffer, DMA filled from
 *       host memory work queues; initialized to all zeros, cleared after
 *       work is taken back to zeros"];
 *
 *       mu_work_despatch_workq [shape=record; label="<tail>|||<head>workq, MU address"];
 *
 *       mu_work_completed [shape=box3d; label="Count of work completed"];
 *
 *     }
 *
 *     subgraph cluster_nfp_work_despatch {
 *       label = "Work Despatch Firmware";
 *       color=blue;
 *
 *       cls_workq_desc [shape=box3d; label="PCIe4 CLS work queue
 *         descriptors"];
 *
 *       workq_me_lmem [shape=box3d; label="Workq ME local memory
 *         cache of work queue descriptors"];
 *
 *       workq_me_enables [ label="Workq ME register with bit-per work
 *         queue enable"];
 *
 *       node [shape=doubleoctagon];
 *
 *       workq_manager [label="Workq_manager\nReads work queue
 *         descriptors and determines which work queues are enabled"];
 *
 *       workq_gatherer [label="Work_gatherer\nDMAs work to memory for
 *         enabled work queues\nAdds work items (workq, MU work address) to
 *         MU work despatch work queue"];
 *
 *     }
 * 
 *     subgraph cluster_nfp_workers {
 *       label = "Work Handling Firmware";
 *       color=blue;
 *
 *       work_buffer [shape=box3d; label="Worker's work buffer"];
 *
 *       workq_me_enables [ label="Workq ME register with bit-per work
 *         queue enable"];
 *
 *       node [shape=doubleoctagon];
 *
 *       worker [label="Worker thread (many of these)\nGets work from
 *         MU work queue\nDMAs work from host to worker-local work
 *         buffer\nPerforms work\nDMAs results to host data buffer
 *         (optional) and work queue"];
 *
 *     }
 * 
 *   host_server -> cls_workq_desc [ minlen=1; style=dashed ];
 *   host_server -> host_memory    [ label="Work\ndata,\nwork\nqueue\nentries"];
 *   host_memory -> host_server    [ dir=both; arrowhead=normal; arrowtail=inv; label="Poll\nwork\nqueue\nentry\nfor\ncompletion,\nread\nresults" ];
 *
 *   cls_workq_desc -> workq_manager  [ minlen=1; dir=both; arrowhead=normal; arrowtail=inv ];
 *   workq_manager -> workq_me_lmem;
 *   workq_manager -> workq_me_enables;
 *
 *   workq_me_lmem -> workq_gatherer    [ dir=both; arrowhead=normal; arrowtail=inv ];
 *   workq_me_enables -> workq_gatherer [ dir=both; arrowhead=normal; arrowtail=inv ];
 *
 *   workq_gatherer -> host_memory  [ minlen=1; style=dotted; label="DMA\nread" ];
 *   workq_gatherer -> mu_work_data [ minlen=1; style=dotted; label="DMA\nwrite"  ];
 *   host_memory -> mu_work_data    [ minlen=1; label="DMA\nwork queue entry\nto MU"];
 *
 *   workq_gatherer -> mu_work_despatch_workq:tail [ minlen=1; style=dashed; label="Add\nwork"  ];
 *   worker         -> mu_work_despatch_workq:tail [ minlen=1; style=dashed; label="Add\nthread"  ];
 *   mu_work_despatch_workq:head -> worker [ minlen=1; style=normal; label="Work"  ];
 *
 *   worker -> host_memory       [ minlen=1; style=dotted; label="DMA\nread/write" ];
 *   worker -> work_buffer       [ style=dotted; label="DMA\nwrite/read"  ];
 *   work_buffer -> host_memory  [ minlen=1; dir=both; label="DMA work to MU,\nresults from MU"];
 *
 *   worker -> work_buffer       [ dir=both; label="Work operation" ];
 *
 *   worker -> mu_work_completed  [ minlen=1; style=dotted; label="Atomic\nincrement" ];
 *   mu_work_completed -> workq_gatherer  [ minlen=1; dir=both; arrowhead=normal; arrowtail=inv; label="Atomic\nread" ];
 * }
 * @enddot
 *
 * STILL TO DO - IMPLEMENT CREDITS IN WORKQ MANAGER/GATHERER IN TO THE SYSTEM
 */

/*a Includes
 */
#include <stdint.h>
#include <nfp/me.h>
#include <nfp/mem.h>
#include <nfp/cls.h>
#include <nfp/pcie.h>
#include <nfp/types.h>
#include <nfp.h>
#include <nfp_override.h>
#include "firmware/data_coproc.h"
#include "data_coproc_lib.h"

/*a Defines
 */
#define U32_LINK_SYM(symname,shift) \
    ((uint32_t)(((uint64_t)__link_sym(#symname))>>shift))

/* DMA_MAX_BURST is the largest DMA burst to do
   All DMAs should be 64B aligned on host and MU, so 1kB is fine
 */
#define DMA_MAX_BURST 1024

/*a Static data used globally */
/*t cls_workq 
 */
/**
 * Local cache copy of cluster scratch work queue, in the local memory
 **/
#define DCPRC_MU_WORK_BUFFER_CLEAR_MASK ((1<<16)-1)
__asm {
    .alloc_mem mu_work_buffer emem global (DCPRC_MU_WORK_BUFFER_CLEAR_MASK+1+0x200) 0x10000;
    .alloc_mem cls_workq i4.cls global 0x400 /*sizeof(struct dcprc_cls_workq)*/ 0;
}
static int check_dcprc_cls_workq_is_0x400_long[(sizeof(struct dcprc_cls_workq)==0x400)?1:-1];
static __imem int ctm_scratch[32]; // Scratch thread-local storage

__shared __cls int          cls_mu_work_wptr;
static __shared __lmem struct dcprc_cls_workq cls_workq_cache;
static __shared __lmem uint32_t workq_rptr[DCPRC_MAX_WORKQS];
static __shared uint32_t workq_enables;
static __shared int std_poll_interval;
struct shared_data {
    __cls void *cls_mu_work_wptr_ptr;
    uint64_32_t mu_work_buffer;
    uint32_t    muq_mu_workq;
};
__shared struct shared_data shared_data;
/* MU workq from gatherer to workers */
#define QDEF_MU_WORKQ  dcprc_mu_workq,16,24,i24.emem
MU_QUEUE_ALLOC(QDEF_MU_WORKQ);

/*a Types */
/*f dcprc_worker_get_work */
void
dcprc_worker_get_work(const struct dcprc_worker_me *restrict dcprc_worker_me,
                      __xread struct dcprc_mu_work_entry *restrict mu_work_entry,
                      struct dcprc_workq_entry *restrict workq_entry)
{
    int mu_work_wptr;
    mem_workq_add_thread(dcprc_worker_me->muq_mu_workq, mu_work_entry, sizeof(*mu_work_entry));

    mu_work_wptr = mu_work_entry->mu_ofs;
    // DO NOT DO THIS - mu_work_wptr &= DCPRC_MU_WORK_BUFFER_CLEAR_MASK;
    // one may be tempted to do so, but this has already been done if necessary by the gatherer
    // in particular sometimes the mu_work_wptr is SUPPOSED TO BE beyond DCPRC_MU_WORK_BUFFER_CLEAR_MASK

    for (;;) {
        __xread struct dcprc_workq_entry workq_entry_in;
        mem_read64_s8(&workq_entry_in, dcprc_worker_me->mu_work_buffer_s8, mu_work_wptr*sizeof(struct dcprc_workq_entry), sizeof(workq_entry_in));
        if (workq_entry_in.work.valid_work) {
            *workq_entry = workq_entry_in;
            break;
        }
    }
    return;
}

/*f dcprc_worker_claim_dma */
__shared __emem __declspec(aligned(16) scope(global) export) uint32_t dcprc_worker_dma_credits[4];
#define DCPRC_DMAS_IN_FLIGHT (50)

__intrinsic void
dcprc_worker_claim_dma(int to_pcie, int poll_interval)
{
    __xrw uint32_t data[2];
    uint64_32_t mem_address;
    data[0] = 0;
    data[1] = 1;
    mem_address.uint64 = (uint64_t)&dcprc_worker_dma_credits;
    mem_address.uint32_lo += to_pcie*8;
    mem_atomic_test_add_hl(data,
                    mem_address.uint32_hi,
                    mem_address.uint32_lo,
                    sizeof(data));
    while ((data[1]-data[0])>DCPRC_DMAS_IN_FLIGHT) {
        me_sleep(poll_interval);
        mem_atomic_read_hl(&(data[0]),
                           mem_address.uint32_hi,
                           mem_address.uint32_lo,
                           sizeof(data[0]));
                           }
}

/*f dcprc_worker_release_dma */
__intrinsic void
dcprc_worker_release_dma(int to_pcie)
{
    uint64_32_t mem_address;
    mem_address.uint64 = (uint64_t)&dcprc_worker_dma_credits;
    mem_address.uint32_lo += to_pcie*8;
    mem_atomic_incr_hl(mem_address.uint32_hi, mem_address.uint32_lo);
}

/*f dcprc_worker_write_results */
void
dcprc_worker_write_results(const struct dcprc_worker_me *restrict dcprc_worker_me,
                           const struct dcprc_mu_work_entry *restrict mu_work_entry,
                           const struct dcprc_workq_entry *restrict workq_entry)
{
    __xwrite struct dcprc_workq_entry workq_entry_out;
    uint64_32_t mu_base;
    uint64_32_t cpp_addr;
    uint64_32_t pcie_addr;
    uint32_t dma_size;

    workq_entry_out.__raw[0] = workq_entry->__raw[0];
    workq_entry_out.__raw[1] = workq_entry->__raw[1];
    workq_entry_out.__raw[2] = workq_entry->__raw[2];
    workq_entry_out.__raw[3] = workq_entry->__raw[3] &~ (1<<31);
    mu_base.uint64 = (uint64_t)&ctm_scratch[0];

    mem_write64_hl(&workq_entry_out, mu_base.uint32_hi, mu_base.uint32_lo, sizeof(workq_entry_out));

    cpp_addr = mu_base;
    pcie_addr.uint32_lo = mu_work_entry->host_physical_address_lo;
    pcie_addr.uint32_hi = mu_work_entry->host_physical_address_hi;
    dma_size = sizeof(struct dcprc_workq_entry);

    pcie_dma_buffer(0, pcie_addr, cpp_addr, dma_size, NFP_PCIE_DMA_TOPCI_HI, 0, PCIE_DMA_CFG);
}

/*f gatherer_get_workqs_to_do */
/**
 * @brief Get bitmask of workqs to check for work from
 *
 * @returns Bitmask of workqs that may have work
 *
 * If no host work queues have work (i.e. none are enabled and
 * indicated to have work by the workq manager) then do not busy-poll;
 * backoff for the standard poll interval.
 *
 **/
static __inline int
gatherer_get_workqs_to_do(void)
{
    int workqs_to_do;
    for (;;) {
        workqs_to_do = workq_enables;
        if (workqs_to_do) return workqs_to_do;
        me_sleep(std_poll_interval);
    }
    return 0;
}

/*f gatherer_get_workq */
/**
 * @brief Get workq to handle from bitmask of workqs that had work -
 * checking that they still do first.
 *
 * @param workqs_to_do Bitmask of workqs to check for, from an earlier
 * snapshot of workq_enables
 *
 * @returns Work queue to try to take work from; updated workqs_to_do
 * removing this queue bit
 *
 * The @p workqs_to_do is a snapshot of work queues that had work
 * earlier; to provide a degree of fairness the gatherer will work
 * through this bitmask from the bottom bit up, provided those work
 * queues still have work (i.e. @p workq_enables is ANDed with
 * workqs_to_do).
 *
 * The 'ffs' instruction is used to find the bottom-most bit of the
 * mask. This returns 0 if bit 0 is set, up to 31 if bit 31 is
 * set. Hence this is the number @p workq_to_read.
 *
 * The bitmask @p workqs_to_do has this new queue cleared in its mask,
 * so that the next bit in the snapshot will be handled next.
 *
 */
static __inline int
gatherer_get_workq(int *workqs_to_do)
{
    int workq_to_read;
    *workqs_to_do &= workq_enables;
    if (*workqs_to_do==0)
        return -1;

    __asm {
        ffs[workq_to_read, *workqs_to_do];
    }
    *workqs_to_do &= ~(1<<workq_to_read);
    return workq_to_read;
}

/*f gatherer_get_num_work */
/**
 * @brief Get the number of work items to take from workq_to_read
 *
 * @param workq_to_read Host work queue number to take work from
 *
 * @param rptr Read pointer to fill out, to start reading workq from
 *
 * @param workq_desc Host work queue data (from the cache managed by
 * data_coproc_workq_manager) for the host work queue '@p
 * workq_to_read'
 *
 * @returns Number of work items to take; the global @p workq_rptr
 * will have been updated for @p workq_to_read so that other threads
 * can continue with working on the same queue.
 *
 * Determines if the host workq @workq_to_read is still active (bit 31
 * set of the wptr indicates it has been shut down - although most of
 * the time the workq_enables will be clear for any queue with wptr
 * bit 31 set, this cannot be guaranteed)
 *
 * If the work queue is still active then work out how many items are
 * on the work queue, up to 15. Limit further down to 4 items - this
 * is to promote fairness amongst host work queues (the gatherer will
 * do at most 4 work items per loop per queue). It is worth batching
 * the work queue entry fetches above one work item though - to reduce
 * the DMA overhead - hence batching up to 4 items.
 *
 * Also, to stop a work queue DMA (later) from failing to wrap in the
 * host work queue (it is a circular buffer) when it should, the @p
 * num_work_to_do is prohibited from making wptr wrap beyond 32 items
 * (currently). If the host work queue is at least 32 entries long
 * (which it has to be, but we do not enforce that yet) then @p
 * num_work_to_do will be bounded by the queue length (or less), based
 * on the value of wptr.
 *
 * Once the number of work items has been figured out, the @p
 * workq_rptr can be moved on by this amount.
 *
 * Note that this function must not deschedule as it must be atomic
 * with other gatherer threads.
 *
 */
static __inline int
gatherer_get_num_work(int workq_to_read,
                      int *rptr,
                      struct dcprc_workq_buffer_desc *workq_desc)
{
    int wptr;
    int num_work_to_do;
    int mask;

    wptr = workq_desc->wptr;
    if (wptr&(1<<31))
        return 0;

    *rptr = workq_rptr[workq_to_read];
    num_work_to_do = (wptr - *rptr) & DCPRC_WORKQ_PTR_CLEAR_MASK;
    if (num_work_to_do>4) { num_work_to_do=4; }
    if (((*rptr+num_work_to_do)&~31) != (*rptr&~31)) {
        num_work_to_do = ((*rptr+32)&~31)-*rptr;
    }
    workq_rptr[workq_to_read] = (*rptr+num_work_to_do) & DCPRC_WORKQ_PTR_CLEAR_MASK;
    mask = workq_desc->max_entries-1;
    *rptr = *rptr & mask;
    return num_work_to_do;
}

/*f gatherer_dma_and_give_work */
/**
 * @brief DMA workq entries from wost work queue to MU, and add work to MU work queue
 *
 * @param workq_to_read Host work queue number to take work from
 *
 * @param rptr Read pointer to start reading workq from
 *
 * @param num_work_to_do Number of work items to take from @p workq_to_read
 *
 * @param workq_desc Host work queue data (from the cache managed by
 * data_coproc_workq_manager) for the host work queue '@p
 * workq_to_read'; this contains the correct @p wptr for the queue
 *
 * Determine where in MU to put the work queue entries. This will be
 * in the MU workq buffer, which is managed through an atomic
 * test-and-add in the CLS. The MU workq buffer is not quite a
 * circular buffer - it is just a litle bit more fluffy than that. In
 * particular, this call may overflow the end of what would normally
 * be the circular buffer as @p num_work_to_do items are DMAed to the
 * @p mu_work_wptr, which may be pointing one entry before the end of
 * the standard buffer. Also, the ordering of entries is a bit
 * fluffier - there is perhaps no guarantee that wptr 2 for host work
 * queue 10 will use an MU workq buffer entry earlier than, for
 * example, wptr 3. The aim, though, it so have storage that can be
 * DMAed. The actual MU pointer for each work item is included in the
 * MU workq entry to the workers anyway.
 *
 * DMA the work queue entries from wptr to wptr+num_work_to_do in to
 * the MU workq buffer. Currently we wait for this to complete.
 *
 * Add work to the MU workq for the workers, with each entry being the
 * @p workq_to_read, @p wptr, and MU workq buffer offset where the
 * host work queue entry was DMAed to.
 *
 * The worker can take this information and perform work, and then
 * update the host workq entry (how does it know where exactly this
 * is?)
 *
 */
static __inline void
gatherer_dma_and_give_work(int workq_to_read,
                           int rptr,
                           int num_work_to_do,
                           struct dcprc_workq_buffer_desc *workq_desc)
{
    uint64_32_t cpp_addr;
    uint64_32_t pcie_addr;
    __xrw int mu_work_wptr;
    int wptr;
    uint32_t dma_size;
    int i;

    mu_work_wptr = num_work_to_do;
    cls_test_add(&mu_work_wptr, shared_data.cls_mu_work_wptr_ptr, 0, sizeof(mu_work_wptr));

    mu_work_wptr &= DCPRC_MU_WORK_BUFFER_CLEAR_MASK;

    cpp_addr.uint32_lo = shared_data.mu_work_buffer.uint32_lo + mu_work_wptr*sizeof(struct dcprc_workq_entry);
    cpp_addr.uint32_hi = shared_data.mu_work_buffer.uint32_hi;
    pcie_addr.uint32_lo = workq_desc->host_physical_address_lo + rptr*sizeof(struct dcprc_workq_entry);
    pcie_addr.uint32_hi = workq_desc->host_physical_address_hi;
    dma_size = num_work_to_do * sizeof(struct dcprc_workq_entry);

    pcie_dma_buffer(0, pcie_addr, cpp_addr, dma_size, NFP_PCIE_DMA_FROMPCI_HI, 0, PCIE_DMA_CFG);

    for (i=0; i<num_work_to_do; i++) {
        __xwrite struct dcprc_mu_work_entry mu_work_entry;
        mu_work_entry.host_physical_address_lo = workq_desc->host_physical_address_lo + (rptr+i)*sizeof(struct dcprc_workq_entry);
        mu_work_entry.host_physical_address_hi = workq_desc->host_physical_address_hi;
        mu_work_entry.mu_ofs = mu_work_wptr+i;

        mem_workq_add_work(shared_data.muq_mu_workq, &mu_work_entry, sizeof(mu_work_entry));
    }
}

/*f data_coproc_work_gatherer */
/**
 * @callgraph
 */
void
data_coproc_work_gatherer(void)
{
    for (;;) {
        int workqs_to_do;
        int work_done;
        workqs_to_do = gatherer_get_workqs_to_do();
        
        work_done = 0;
        for (;;) {
            int workq_to_read;
            int rptr;
            struct dcprc_workq_buffer_desc workq_desc;
            int num_work_to_do;

            workq_to_read = gatherer_get_workq(&workqs_to_do);
            if (workq_to_read<0) break;

            workq_desc = cls_workq_cache.workqs[workq_to_read];

            num_work_to_do = gatherer_get_num_work(workq_to_read, &rptr, &workq_desc);
            if (num_work_to_do==0)
                continue;

            gatherer_dma_and_give_work(workq_to_read, rptr, num_work_to_do,
                                       &workq_desc);
            work_done = 1;
        }

        if (!work_done) {
            me_sleep(std_poll_interval);
        }
    }
}

/*f data_coproc_workq_manager */
/**
 * @callgraph
 */
void
data_coproc_workq_manager(int max_queue)
{
    __cls void *cls_workq_base;
    
    int workq_to_read=0;
    int workq_bit = 1;
    cls_workq_base = (__cls void *)U32_LINK_SYM(cls_workq,0);
    for (;;) {

        __xread struct dcprc_workq_buffer_desc cls_buffer_desc;
        int ofs;

        ofs = sizeof(struct dcprc_workq_buffer_desc)*workq_to_read;
        cls_read(&cls_buffer_desc, cls_workq_base, ofs, sizeof(cls_buffer_desc));
        cls_workq_cache.workqs[workq_to_read] = cls_buffer_desc;

        if (cls_buffer_desc.wptr&(1<<31)) {
            workq_enables &= ~workq_bit;
        } else if (cls_buffer_desc.wptr != workq_rptr[workq_to_read]) {
            workq_enables |= workq_bit;
        } else {
            workq_enables &= ~workq_bit;
        }
        workq_to_read = (workq_to_read+1);
        workq_bit = workq_bit<<1;
        if (workq_to_read>=max_queue) {
            workq_to_read = 0;
            workq_bit = 1;
        }

    }
}

/*f data_coproc_init_workq_gatherer
 */
void
data_coproc_init_workq_gatherer(void)
{
    shared_data.muq_mu_workq  = MU_QUEUE_CONFIG_GET(QDEF_MU_WORKQ);
}

/*f data_coproc_init_workq_manager
 */
void
data_coproc_init_workq_manager(int poll_interval)
{
    int i;
    std_poll_interval = poll_interval;
    shared_data.muq_mu_workq   = MU_QUEUE_CONFIG_WRITE(QDEF_MU_WORKQ);
    shared_data.mu_work_buffer.uint64 = __link_sym("mu_work_buffer");
    shared_data.cls_mu_work_wptr_ptr = (void *)&cls_mu_work_wptr;
    workq_enables = 0;
    for (i=0; i<DCPRC_MAX_WORKQS; i++) {
        cls_workq_cache.workqs[i].max_entries=0;
    }
}

/*f dcprc_worker_init */
__intrinsic void
dcprc_worker_init(struct dcprc_worker_me *restrict dcprc_worker_me)
{
    dcprc_worker_me->muq_mu_workq   = MU_QUEUE_CONFIG_GET(QDEF_MU_WORKQ);
    dcprc_worker_me->mu_work_buffer_s8 = U32_LINK_SYM(mu_work_buffer,8);
}

