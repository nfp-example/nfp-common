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
/*a Types */
/*f do_work */
static void
do_work(int queue_number)
{
//    get_next_mu_work_address(and add up to 4);
//    start_dma_of_up_to_four_desc_from_workq();
//    add_up_to_four_to_rptr();
//    add_work_to_internal_workqueue(); // work is queue number, MU work address (16B there)
    // use credits for internal work queue
    // have a cache of credits in ME
    // Out of kindness, atomically add number to 'write ptr' in MU when adding work
    // 'completed_ptr' incremented when workers complete all the processing
    // cache that completed_ptr in a caching thread
    // caching thread can also manage which queues are active
    // THAT should be the workq manager...
    // work gatherer should DMA work from host to MU...
}

/*f data_coproc_workq_manager */
/**
 * @callgraph
 *  @dot
 *  digraph workq_manager {
 *      node [shape=record, fontname=Helvetica, fontsize=10];
 *      a [ label="work_in_hand?" URL="\ref B"];
 *      b [ label="read_write_pointer" URL="\ref B"];
 *      c [ label="determine if work ready" URL="\ref C"];
 *      d [ label="Work ready?" URL="\ref C"];
 *      e [ label="SLEEP" URL="\ref C"];
 *      f [ label="launch_work" URL="\ref C"];
 *      a -> b, f;
 *      b -> c;
 *      c -> d;
 *      d -> e, f;
 *      e -> a;
 *      f -> a;
 *  }
 *  @enddot
 */
__intrinsic void
data_coproc_workq_manager(void)
{
    /*    for (;;) {
        if (!has_work) {
            read_wptr();
            has_work = (wptr!=rptr);
            if (!has_work) {
                sleep;
                continue;
            }
        }
        if (wptr<0) {
            disable_queue();
            sleep_forever();
        }
        do_work();
        }*/
}

/** data_coproc_init_workq_manager
 */
__intrinsic void
data_coproc_init_workq_manager(void)
{
//    read_workq_desc_into_lmem();
//    init_cls_workq();
}

