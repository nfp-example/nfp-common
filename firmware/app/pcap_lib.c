/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 *
 * @file        pcap_lib.c
 * @brief       A simple packet capture system
 *
 * This is a library to support a PCAP packet capture to a host x86
 * system
 * 
 * The host delivers buffers to the CLS of the PCI island; it has a
 * software ring with a write ptr written by the host. The software
 * ring contains entries of the form 64-bit PCIe address. A thread
 * reads these buffers from the hosts and pairs them with internal MU
 * buffers. The MU buffers are used to hold received packets, and the
 * buffer contents are DMAed into the paired host buffer.
 *
 * Packets are received by the hardware solely into CTM, and then have
 * to be DMAed in to an portion of one of the MU buffers. The MU
 * buffer is distributed using a buffer allocation system based on
 * contiguously allocating memory for the packets ordered by the
 * allocation requests.
 *
 * The MU buffer allocation uses a simple structure containing the
 * base address in MU addressing of the buffer base, a packet number
 * and the next offset to be allocated.
 *
 * An allocation is performed by incrementing the packet number and
 * adding the allocation to the buffer offset. This is performed with
 * a saturating add operation, of length 2 32-bit words. The packet
 * number and buffer offset are placed as the top-most fields of the
 * two 32-bit words, and so saturation protects from overflow.
 *
 * Using a test-and-add-sat provides the allocator with the allocation
 * while moving the buffer descriptor on appropriately.
 *
 * Using 256kB MU buffers aligned to 256kB requires a 22-bit MU address,
 * and it can take 128 2kB packets or 2k 64B packets.
 *
 * The front of the MU buffer contains:
 *  A bit mask of the packets that have completed DMA (atomic sets)
 *  Descriptors of the packets that have completed DMA (bulk writes)
 * 
 * The host buffers must be multiples of 256kB as they are paired with
 * the 256kB MU buffers.
 *
 * The to-host DMA process consists of a master thread which takes
 * control of an MU buffer and it monitors the bit-mask of packets
 * ready. It then packages up DMAs: PCIe base, MU base, first packet,
 * last packet, first offset, last offset, and presents them as work
 * to slave DMA threads When the MU buffer is completed and all the DMA
 * threads complete the MU buffer can be recycled.
 * 
 * The buffer recycling thread manages the 256kB PCIe buffer
 * allocation, using a CLS memory as a ring. The buffers from the host
 * are consumed in the order that they are presented. It pairs the
 * PCIe buffers with MU buffers from a free pool. Before the MU buffer
 * is presented for allocation its header must be zeroed as that
 * contains the bit-mask of ready packets.
 *
 */

/** Includes
 */
#include <stdint.h>
#include <nfp/me.h>
#include <nfp/mem.h>
#include <nfp/cls.h>
#include <nfp/pcie.h>
#include <nfp/types.h>
#include <nfp.h>
#include <nfp_override.h>
#include "firmware/pcap.h"
#include "pcap_lib.h"

/** Defines
 */
#define U32_LINK_SYM(symname,shift) \
    ((uint32_t)(((uint64_t)__link_sym(#symname))>>shift))

#define CTM_PKT_OFFSET (32)

/* Note that MU_BUF_TOTAL_PKTS MUST NOT exceed the 'number' field in
   the mu_buf_desc*/
#define MU_BUF_TOTAL_PKTS 1024

/* MU_BUF_MAX_PKT must be a little less than MU_BUF_TOTAL_PKTS -
 possibly one less would be sufficient
*/
#define MU_BUF_MAX_PKT (MU_BUF_TOTAL_PKTS-4)

/* MU_BUF_FIRST_PKT_OFFSET must be greater than
 * 64B+(MU_BUF_TOTAL_PKTS/8)+MU_BUF_MAX_PKT*sizeof(mu_pkt_buf_desc)
 * 64 + 128 + 1020*8
 *
 * Since the latter dominates, 16*MU_BUF_MAX_PKT is fine... it wastes
 * a bit of the buffer but not much
 */
#define MU_BUF_FIRST_PKT_OFFSET (16*1024)

/* MAX_CTM_DMAS_IN_PROGRESS is the maximum number of CTM DMAs that the
 * hardware supports
 *
 */
#define MAX_CTM_DMAS_IN_PROGRESS 16

/* DMA_MAX_BURST is the largest DMA burst to do
   All DMAs should be 64B aligned on host and MU, so 1kB is fine
 */
#define DMA_MAX_BURST 1024

/* PKT_BUF_* is the return value from pkt_buffer_alloc_from_current
 */
enum {
    PKT_BUF_NOT_INIT,
    PKT_BUF_OVERFLOWED,
    PKT_BUF_ALLOCKED
};

/** Static data used globally
 */
/* mu_buf_desc_store : struct mu_buf_desc */
_alloc_mem("mu_buf_desc_store emem global 8")

/** Queue descriptors and allocations
 */
/* Recycle queue is workq of mu_base_s8 */
#define QDEF_MU_BUF_RECYCLE pcap_mu_buf_recycle,10,16,emem

/* Buf in use is workq of mu_base_s18 */
#define QDEF_MU_BUF_IN_USE  pcap_mu_buf_in_use,10,17,emem

/* Buf alloc is workq of struct mu_buf_desc (8 bytes) */
#define QDEF_MU_BUF_ALLOC   pcap_mu_buf_alloc,11,18,emem

/* To host DMA is workq of struct mu_buf_to_host_dma_work (8 bytes) */
#define QDEF_TO_HOST_DMA    pcap_to_host_dma,11,19,emem

MU_QUEUE_ALLOC(QDEF_MU_BUF_RECYCLE);
MU_QUEUE_ALLOC(QDEF_MU_BUF_IN_USE);
MU_QUEUE_ALLOC(QDEF_MU_BUF_ALLOC);
MU_QUEUE_ALLOC(QDEF_TO_HOST_DMA);

/** Memory allocation for packet receive threads
 */
#ifdef PCAP_RX_ISLAND
/* cls_ctm_dmas : struct cls_ctm_dma_credit */
_alloc_mem("cls_ctm_dmas cls island 8")
#endif

/** Memory allocation for host interaction threads
 */
#ifdef PCAP_HOST_ISLAND
#define __STR(a) #a
#define STR(a) __STR(a)
#define ALLOC_MEM(res,mem,scope,size) \
    _alloc_mem(STR(res) " " STR(mem) " " STR(scope) " " STR(size))
#define ALLOC_RES(name,pool,scope,size) \
    __alloc_resource(STR(name) " " STR(pool) " " STR(scope) " " STR(size))

ALLOC_MEM(pcap_cls_host_shared_data,cls,global,PCAP_HOST_CLS_SHARED_DATA_SIZE)
ALLOC_MEM(pcap_cls_host_ring_base,  cls,global,PCAP_HOST_CLS_RING_SIZE)
#endif /* PCAP_HOST_ISLAND */

#define ALLOC_CLS_HOST_RING() ALLOC_RES(chr,cls_host_ring_base,island,64)

/** Static data for all thread types
 *
 * The compiler optimizes out unneeded regs
 */
static uint32_t mu_buf_desc_store_s8; /* For packet rx */
static uint32_t cls_ctm_dmas;         /* For packet rx */
static __declspec(shared) int packet_count; /* For packet rx */

/* muq_mu_buf_recycle is used by fill, DMA master, MU recycler */
static __declspec(shared) uint32_t muq_mu_buf_recycle;

/* muq_mu_buf_alloc is used by MU recycler and packet rx */
static __declspec(shared) uint32_t muq_mu_buf_alloc;

/* muq_mu_buf_in_use is used by packet rx and DMA master */
static __declspec(shared) uint32_t muq_mu_buf_in_use;

/* muq_to_host_dma is used by DMA master and slaves */
static __declspec(shared) uint32_t muq_to_host_dma;

/** struct cls_ctm_dma_credit
 *
 * Stored in CLS, this is a CTM DMA credit management structure. It is
 * initialized to zero, then the number of outstanding CTM DMAs is
 * next_claimant-last_complete. A thread wishing to do a DMA
 * test-and-increments next_claimant, getting its claimant
 * number. When its claimant number is less than
 * last_complete+max_ctm_dmas it may DMA. When it completes it
 * increments last_complete.
 *
 */
struct cls_ctm_dma_credit {
    int next_claimant;
    int last_complete;
};

/** struct ctm_pkt_hdr
 *
 * Hardware packet info as delivered by the CTM, originally from the
 * NBI DMA (see MU packet engine in databook)
 *
 */
struct ctm_pkt_hdr {
    unsigned int ctm_island:6;
    unsigned int pkt_num:10;
    unsigned int blist:2;
    unsigned int length:14;

    unsigned int split:1;
    unsigned int pad:2;
    unsigned int mu_base_s11:29;

    unsigned int buf_pool:8; /* From metadata */
    unsigned int pad2:8;
    unsigned int seq:16;
};

/** struct pkt_buf_desc
 *
 * Packet buffer descriptor held within a thread, with fields filled
 * in as and when they are available
 *
 */
struct pkt_buf_desc {
    uint32_t num_blocks;
    uint32_t mu_base_s8;
    uint32_t mu_offset;
    uint32_t mu_num;
    uint32_t seq;
    uint32_t pkt_num;
    uint32_t pkt_addr;
};

/** struct mu_pkt_buf_desc
 *
 * Packet buffer descriptor stored in the MU buffer.  The offset
 * is the 64B block offset from mu_base_s8.  num_blocks is the number
 * of 64B block spaces used in the MU buffer for the packet. The
 * sequence number is a 16/32-bit sequence number of the packet, as
 * supplied by the NBI Rx.
 *
 */
struct mu_pkt_buf_desc {
    uint32_t offset:16;
    uint32_t num_blocks:16;
    uint32_t seq;
};

/** struct mu_buf_hdr
 *
 *  Structure placed at start of an MU buffer
 *
 */
struct mu_buf_hdr {
    uint32_t buf_seq;         /* MU buffer sequence number */
    uint32_t total_packets;   /* Valid when buffer is complete */
    uint32_t pcie_base_low;   /* Filled at pre-allocation */
    uint32_t pcie_base_high;  /* Filled at pre-allocation */
};

/** struct mu_buffer
 *
 * MU buffer layout, up to the packet data, which is placed at
 * MU_BUF_FIRST_PKT_OFFSET
 *
 * Note that this must be less than MU_BUF_FIRST_PKT_OFFSET in size
 * Note also that the pkt_add_mu_buf_desc clears this structure in a
 * 'knowledgeable manner', i.e. it knows the structure and offsets
 * intimately. So changing this structure requires changing that
 * function.
 */
struct mu_buffer {
    struct mu_buf_hdr hdr;
    int      dmas_completed;  /* For DMA Master/slaves */
    uint32_t pad[11];         /* Pad to 64B alignment */
    uint32_t pkt_bitmask[MU_BUF_TOTAL_PKTS/32]; /* n*64B to pad
                                                 * properly */
    struct mu_pkt_buf_desc pkt_desc[MU_BUF_MAX_PKT];
};

/** struct mu_buf_desc
 *
 * MU buffer descriptor for the allocation system
 *
 * mu_base_s18 is the base address in MU of the buffer (>>18)
 * offset is the offset in 64Bs from the start of MU buffer to the next
 *     available spot for allocation
 * number is the next packet number for the MU buffer
 *
 */
struct mu_buf_desc {
    union {
        struct {
            unsigned int offset:24;
            unsigned int pad:8;
            unsigned int number:10;
            unsigned int mu_base_s18:22;
        };
        int64_t __raw;
    };
};

/** struct mu_buf_to_host_dma_work
 *
 * MU buffer DMA work descriptor, giving MU buffer base address and
 * which packets to DMA
 *
 */
struct mu_buf_to_host_dma_work {
    uint32_t mu_base_s8;
    int first_packet:16;
    int num_packets:16;
};

/** struct mu_buf_dma_desc
 *
 * MU buffer DMA descriptor used in the PCIe DMA slave, containing the
 * complete descriptor of the work to be done
 *
 */
struct mu_buf_dma_desc {
    uint32_t mu_base_s8;
    uint32_t pcie_base_low;
    uint32_t pcie_base_high;
    int first_packet;
    int num_packets;
    int first_block; /* Inclusive, first block */
    int end_block; /* Exclusive (last byte is end_block-1) */
};

/** struct host_data
 *
 * Data needed in the PCIe host buffer gather thread
 *
 */
struct host_data {
    uint32_t cls_host_shared_data;
    uint32_t cls_host_ring_base;
    uint32_t cls_host_ring_item_mask;
    uint32_t wptr;
    uint32_t rptr;
};

/** struct pcie_buf_desc
 *
 * PCIe buffer descriptor, as passed in to the firmware through the CLS
 *
 */
struct pcie_buf_desc {
    uint32_t pcie_base_low;
    uint32_t pcie_base_high;
};

/** nbi_give_two_buffers - 6i
 * 6 inst, 1 cls read no wait
 *
 * Provide two buffers to NBI, not caring what they are
 * Only works if the NBI DMA Rx never splits into MU buffer
 *
 * Issues a CLS read to the NBI DMA master/ref required
 *
 */
static __inline void
nbi_give_two_buffers()
{
    int dm;       /* Data master override data */
    int override; /* Override data for command indirect_ref */
    int zero = 0; /* Address in CLS to read - can be anything */
    volatile __xread uint32_t unused[2]; /* unused xfer registers */

    /* NBI DMA is data master 2; set indirect CSR to be island+DMA
     */
    dm = (PKT_CAP_NBI_ISLAND << 24) | (2 << 20);
    local_csr_write(local_csr_cmd_indirect_ref0, dm);

    override = ((1<<1) | (1<<3)); /* Override DM, DREF */
    __asm {
        alu[ --, --, B, override ];
        cls[read, unused, zero, 0, 2], indirect_ref;
    }
}

/** pkt_mu_buf_desc_taken - 5i + 100d
 * 5 inst, 1 mu workq add
 * 
 * Indicate that an MU buffer has been taken to use for allocation.
 * 
 * @param mu_buf_desc   MU buffer decsriptor that has been filled
 * 
 */
static __inline void
pkt_mu_buf_desc_taken(struct mu_buf_desc *mu_buf_desc)
{
    __xwrite uint32_t wdesc; /* Xfer for work item for MU workq */

    wdesc = mu_buf_desc->mu_base_s18;
    mem_workq_add_work(muq_mu_buf_in_use, &wdesc, sizeof(wdesc));
}

/** pkt_mu_buf_desc_complete - 6i + 100d
 * 6 inst, 1 mu buf write
 * 
 * Indicate that an MU buffer has been fully allocated. This is done
 * by writing the total_packets field of the MU buffer descriptor
 * header.
 * 
 * @param mu_buf_desc   MU buffer decsriptor that has been filled
 * 
 */
static __inline void
pkt_mu_buf_desc_complete(struct mu_buf_desc *mu_buf_desc)
{
    __xwrite uint32_t total_packets;  /* Total packets in MU buf */
    uint32_t mem_base_s8;             /* MU buf base>>8 */
    uint32_t mem_offset;              /* Offset to MU buf of total_packets
                                         structure element */

    total_packets = mu_buf_desc->number;
    mem_base_s8 = mu_buf_desc->mu_base_s18<<10;
    mem_offset = offsetof(struct mu_buf_hdr,total_packets);
    mem_atomic_write_s8(&total_packets, mem_base_s8, mem_offset,
                        sizeof(uint32_t));
}

/** pkt_work_enq - 16i + 100d
 * 16 inst, 2 parallel mu buf write+atomic set
 * 
 * Set the MU buffer descriptor to indicate that a packet has been
 * received fully. Writes the descriptor and sets the relevant bit.
 *
 * There is a subtle potential race condition here, between the write
 * of the packet buffer descriptor, and the packet 'ready' bitmask bit
 * being set, the DMA master reading it, then the DMA master then
 * reading the packet buffer descriptor.
 *
 * This could be avoided if the mem[write] and mem[set] were serialized
 *
 * However, this seems quite unnecessary at this point.
 * 
 * @param pkt_buf_desc      Packet buffer descriptor to mark
 * 
 */
pkt_work_enq(struct pkt_buf_desc *pkt_buf_desc)
{
    SIGNAL sig1, sig2; /* Signals for parallel MU transactions */
    struct mu_pkt_buf_desc mu_pkt_buf_desc;
    __xwrite struct mu_pkt_buf_desc mu_pkt_buf_desc_out;
    uint32_t mu_base_s8;     /* Base address of MU buffer >> 8 */
    uint32_t mu_desc_offset; /* Offset to pkt_buf_desc in MU buffer */
    uint32_t mu_bit_offset;  /* Offset to word in bitmask for the packet
                                mu_num in MU buffer */
    __xwrite uint32_t mu_bit_out; /* Bit to set in bitmask for the packet
                                     mu_num */

    mu_pkt_buf_desc.offset     = pkt_buf_desc->mu_offset>>6;
    mu_pkt_buf_desc.num_blocks = pkt_buf_desc->num_blocks;
    mu_pkt_buf_desc.seq        = pkt_buf_desc->seq;

    mu_base_s8 = pkt_buf_desc->mu_base_s8;
    mu_bit_offset = (offsetof(struct mu_buffer,pkt_bitmask) +
                     ((pkt_buf_desc->mu_num / 32) << 2));
    mu_desc_offset = (offsetof(struct mu_buffer,pkt_desc) +
                      pkt_buf_desc->mu_num * sizeof(struct mu_pkt_buf_desc));

    mu_bit_out = 1 << (pkt_buf_desc->mu_num & 31);
    mu_pkt_buf_desc_out = mu_pkt_buf_desc;
    __asm {
        mem[write, mu_pkt_buf_desc_out, mu_base_s8, <<8, \
            mu_desc_offset, 1], sig_done[sig1];
        mem[set, mu_bit_out, mu_base_s8, <<8, \
            mu_bit_offset, 1], sig_done[sig2];
    }
    wait_for_all(&sig1, &sig2);
}

/** pkt_free - 7i
 * 4 inst + 50% nbi_give_two_buffers
 *
 * Free a CTM packet buffer, and give NBI DMA Rx buffers if necessary
 *
 */
static __inline void
pkt_free(struct pkt_buf_desc *pkt_buf_desc)
{
    uint32_t pkt_num; /* Packet number in the CTM, to be freed */

    pkt_num = pkt_buf_desc->pkt_num;
    __asm { mem[packet_free, --, pkt_num, 0] }
    if ((packet_count & 1) == 0) {
        nbi_give_two_buffers();
    }
    packet_count++;
}

/** pkt_receive - 10i + 200d
 * 10 inst, 2 ctm read
 *
 * Take next received packet from CTM, get packet number, address and
 * size in blocks
 * 
 */
static __intrinsic void
pkt_receive(struct pkt_buf_desc *pkt_buf_desc)
{
    uint32_t zero = 0; /* Zero register (command needs a reg operand) */
    uint32_t pkt_hdr_words; /* Size of ctm_pkt_hdr in words */
    SIGNAL   sig;           /* Signal needed for CPP commands */
    __xread struct ctm_pkt_hdr pkt_hdr; /* Packet header of received
                                           packet */
    uint32_t pkt_num;  /* Packet number of the packet in the CTM */
    __xread uint32_t pkt_status[2]; /* Packet status, including CTM
                                       address of packet */

    pkt_hdr_words = sizeof(pkt_hdr) >> 2;
    __asm {
        mem[packet_add_thread, pkt_hdr, zero, 0, pkt_hdr_words], \
            ctx_swap[sig]
            }

    pkt_num = pkt_hdr.pkt_num;
    pkt_buf_desc->pkt_num = pkt_hdr.pkt_num;
    pkt_buf_desc->seq  = pkt_hdr.seq;
    pkt_buf_desc->num_blocks = (pkt_hdr.length+CTM_PKT_OFFSET+63)>>6;
    __asm {
        mem[packet_read_packet_status, pkt_status[0], pkt_num, 0, 1], \
            ctx_swap[sig]
            }
    pkt_buf_desc->pkt_addr = ((pkt_status[0]&0x3ff)<<8)|CTM_PKT_OFFSET;
}

/** pkt_buffer_alloc_from_current - 30i + 150d
 * 30 inst best case, 1 mu_buf_alloc atomic
 * 
 * Attempt to allocate a buffer of the required size from the current
 * MU buffer descriptor.
 *
 * Try to allocate the size, and one packet.
 * If allocation succeeds, then return PKT_BUF_ALLOCKED
 *
 * If the buffer descriptor is not initialized AND this is the first
 * claimant then return PKT_BUF_INIT
 *
 * If allocation fails AND this is the first clamant to fail, return
 * PKT_BUF_OVERFLOWED
 *
 * Otherwise sleep and retry
 *
 */
static __inline int
pkt_buffer_alloc_from_current(struct mu_buf_desc *mu_buf_desc,
                              struct pkt_buf_desc *pkt_buf_desc,
                              int poll_interval)
{
    for (;;) {
        SIGNAL_PAIR atomic_sig; /* Signals for test_addsat */
        /* Data to modify the mu_buf_desc_store with */
        struct mu_buf_desc atomic_buffer_desc;
        /* Write and premodified data from mu_buf_desc_store */
        __xrw struct mu_buf_desc atomic_buffer_desc_rw;

        int buffer_end;      /* Last offset in MU buffer */
        int pkt_starts_okay; /* True: alloc starts in MU buffer */
        int pkt_ends_okay;   /* True: alloc ends in MU buffer */
        int pkt_num_okay;    /* True: alloc mu_num of pkt is allowed
                                in MU buffer */
        int pkt_num_max;     /* True: mu_num pkt is max allowed in
                                MU buffer */

        atomic_buffer_desc.__raw = 0;
        atomic_buffer_desc.number = 1;
        atomic_buffer_desc.offset = pkt_buf_desc->num_blocks;

        atomic_buffer_desc_rw = atomic_buffer_desc;
        __asm {
            mem[test_addsat, atomic_buffer_desc_rw, \
                mu_buf_desc_store_s8, <<8, 0, 2], sig_done[atomic_sig];
                }
        /* Cannot do ctx_swap as this is a signal pair: wait_for_all */
        wait_for_all(&atomic_sig);

        /* Check for uninitialized buffer - if first allocator, then
           return PKT_BUF_NOT_INIT, else wait and restart, polling
           until first allocator has filled mu_buf_desc_store
        */
        *mu_buf_desc = atomic_buffer_desc_rw;
        if (mu_buf_desc->mu_base_s18 == 0) {
            if (mu_buf_desc->offset == 0) return PKT_BUF_NOT_INIT;
            me_sleep(poll_interval);
            continue;
        }
        buffer_end = 1 << 18; /* Buffer size fixed at 256kB, 1<<18 */

        pkt_starts_okay = (mu_buf_desc->offset <= buffer_end);
        pkt_ends_okay   = ((mu_buf_desc->offset + pkt_buf_desc->num_blocks)
                           <= buffer_end);
        pkt_num_okay    = (mu_buf_desc->number < MU_BUF_MAX_PKT);
        pkt_num_max     = (mu_buf_desc->number == MU_BUF_MAX_PKT);

        /* If a good allocation then return
         */
        if (pkt_ends_okay && pkt_num_okay) {
            pkt_buf_desc->mu_num     = mu_buf_desc->number;
            pkt_buf_desc->mu_base_s8 = mu_buf_desc->mu_base_s18 << 10;
            pkt_buf_desc->mu_offset  = mu_buf_desc->offset << 6;
            return PKT_BUF_ALLOCKED;
        }

        /* Last allocation WAS good if it was <=(pkt_num_max-1) and it
         * ended before the MU buffer end, i.e. this allocation
         * 'starts okay'
         *
         * If last allocation was okay, this allocation is the
         * overflow which forces a new MU buffer to be used
         */
        if (pkt_starts_okay) {
            if (pkt_num_max || pkt_num_okay)
                return PKT_BUF_OVERFLOWED;
        }

        /* This allocation failed, last also failed, so retry required
         * - another context should be setting up the new MU buffer
         */
        me_sleep(poll_interval);
    }
}

/** pkt_buffer_alloc_from_new - 17i + 350d
 * 12 inst, 1 mu workq add thread, 1 mu atomic write,
 * pkt_mu_buf_desc_taken
 *
 * Allocate a packet buffer descriptor from a new MU buffer
 * descriptor, taken from the MU buffer work queue (from the
 * recycler).
 *
 */
static __inline void
pkt_buffer_alloc_from_new(struct mu_buf_desc *mu_buf_desc,
                          struct pkt_buf_desc *pkt_buf_desc)
{
    __xread  struct mu_buf_desc mu_buf_desc_read;
    __xwrite struct mu_buf_desc mu_buf_desc_write;

    mem_workq_add_thread(muq_mu_buf_alloc, (void *)&mu_buf_desc_read,
                         sizeof(mu_buf_desc_read));
    *mu_buf_desc = mu_buf_desc_read;
    pkt_mu_buf_desc_taken(mu_buf_desc);

    pkt_buf_desc->mu_base_s8 = mu_buf_desc->mu_base_s18 << 10;
    pkt_buf_desc->mu_offset  = MU_BUF_FIRST_PKT_OFFSET;
    pkt_buf_desc->mu_num     = mu_buf_desc->number;

    mu_buf_desc->offset = ((MU_BUF_FIRST_PKT_OFFSET >> 6) +
                           pkt_buf_desc->num_blocks);
    mu_buf_desc->number = 1;

    mu_buf_desc_write = *mu_buf_desc;
    mem_atomic_write_s8(&mu_buf_desc_write, mu_buf_desc_store_s8, 0,
                        sizeof(mu_buf_desc_read));
}

/** pkt_buffer_alloc - 40i + 157d 
 * pkt_buffer_alloc_from_current + 10i + 2% pkt_buffer_alloc_from_new
 * 
 * @param pkt_buf_desc   Packet buffer with filled-in num_blocks
 * @param poll_interval  Sleep interval if backoff required
 *
 * Allocate from the current MU buffer if possible; on overflow,
 * complete the last descriptor, and overflow or init requires a new
 * buffer allocation from the MU buffer recycler
 */
static void
pkt_buffer_alloc(struct pkt_buf_desc *pkt_buf_desc, int poll_interval)
{
    struct mu_buf_desc mu_buf_desc; /* MU buffer for the packet */
    int alloc;                      /* Allocation result */

    alloc = pkt_buffer_alloc_from_current(&mu_buf_desc, pkt_buf_desc,
                                          poll_interval);

    if (alloc==PKT_BUF_ALLOCKED) return;
    if (alloc==PKT_BUF_OVERFLOWED) {
        pkt_mu_buf_desc_complete(&mu_buf_desc);
    }

    pkt_buffer_alloc_from_new(&mu_buf_desc, pkt_buf_desc);
}

/** pkt_dma_to_memory - 16i + 50d + 100d + DMA time (100d+pktB/4)
 * 16 inst + cls atomic + cls incr + ctm dma
 *
 * @param pkt_buf_desc
 *
 * Uses the CTM packet engine DMA to DMA a region of CTM SRAM to MU
 *
 * Note that at most 16 DMAs can be in progress at once.  For this we
 * use a 32-bit lagging counter and a 32-bit claim counter.  To claim,
 * test-and-increment the claim counter while reading the lagging
 * counter.  If the claim counter - lagging counter (=consumed credit)
 * is less than or equal to 16 then permission is given to DMA; else
 * poll until this is the case.  After DMA completion, increment the
 * lagging counter.
 *
 * Uses the CTM command pe_dma_to_memory_buffer
 * - length      = (size in 64B)-1
 * - byte_mask   = top 8 bits of MU address
 * - address     = bottom 32 bits of MU address (8B aligned)
 * - data_master = top bit of (CTM address>>3)
 * - data_ref    = bottom 14 bits of (CTM address>>3)
 * - signals when the DMA completes
 * 
 * Need to override the byte_mask, data_master/data_ref, length
 * - The byte_mask must go in cmd_indirect_ref_0, bottom 8 bits
 * - prev_alu must have OVE_DATA=2 (master/ref in DATA=[16;16])
 * - prev_alu must have OV_LEN (length in LENGTH=[5;8])
 */
static __intrinsic
void pkt_dma_to_memory(struct pkt_buf_desc *pkt_buf_desc,
                       int poll_interval)
{
    SIGNAL sig;
    uint32_t override; 
    uint32_t mu_addr_high; /* Top 8 bits of MU buffer address */
    uint32_t mu_addr_low;  /* Bottom 32 bits of MU buffer address */
    uint32_t mu_offset;    /* Offset in to MU buffer for packet */
    int      size;         /* Size of DMA in 64B lumps */
    __xrw uint32_t data[2]; /* R/W xfer registers for CTM claim */

    /* Claim credit of one of CTM DMAs
       Returns with data[0] = last released, data[1] = claim number
     */
    data[0]=0;
    data[1]=1;
    __asm {
        cls[test_add, data[0], cls_ctm_dmas, 0, 2], ctx_swap[sig];
    }
    while ((data[1] - data[0]) >= MAX_CTM_DMAS_IN_PROGRESS) {
        me_sleep(poll_interval);
        cls_read(&data[0], (__cls void *)cls_ctm_dmas, 0,
                 sizeof(uint32_t));
    }

    mu_addr_high = (pkt_buf_desc->mu_base_s8) >> 24;
    mu_addr_low  = pkt_buf_desc->mu_base_s8 << 8;
    mu_offset    = pkt_buf_desc->mu_offset << 6;
    local_csr_write(local_csr_cmd_indirect_ref0, mu_addr_high); 
    size         = pkt_buf_desc->num_blocks+1;
    override = (( (2 << 3) | (1 << 6) | (1 << 7) ) | ((size - 1) << 8) |
                (pkt_buf_desc->pkt_addr << (16 - 3)));
    __asm {
        alu[ --, --, B, override ];
        mem[pe_dma_to_memory_buffer, --, mu_addr_low, mu_offset, 1], \
            indirect_ref, ctx_swap[sig]; 
    }

    /* Release credit of one of CTM DMAs
     */
    cls_incr((__cls void *)cls_ctm_dmas, 0);
}

/** packet_capture_pkt_rx_dma - 89i + 707d + pktB/4
 * 10i + 200d + 40i + 157d + 16i + 150d + 100d + pktB/4 + 16i + 100d + 7i
 * Estimate of 1 thread does 1Mpps max, 8 thread can run per ME
 * 
 * Need 64 threads for min pkt 40GbE, spread across many CTM for
 * bandwidth sharing Could remove DMA credit handling if <64 threads
 * per CTM
 *
 * Handle packet received by CTM; claim next part of MU buffer, DMA
 * the packet in, then pass on to work queue and free the packet
 *
 */
void packet_capture_pkt_rx_dma(int poll_interval)
{
    mu_buf_desc_store_s8 = U32_LINK_SYM(mu_buf_desc_store, 8);
    cls_ctm_dmas         = U32_LINK_SYM(cls_ctm_dmas, 0);
    for (;;) {
        struct pkt_buf_desc pkt_buf_desc;

        pkt_receive(&pkt_buf_desc);
        pkt_buffer_alloc(&pkt_buf_desc,poll_interval);
        pkt_dma_to_memory(&pkt_buf_desc,poll_interval);
        pkt_work_enq(&pkt_buf_desc);
        pkt_free(&pkt_buf_desc);
    }
}

/** pkt_dma_memory_to_host - n*(28i + 100d + 500d (2kB) / 150d (500B))
 * (28i + pcie internal write + PCIE dma) per 2kB
 * 2kB=16kbits PCIe DMA at PCIe gen 3 (64Gbps max) = approx 500 cycles
 *
 * Split into DMA_MAX_BURST size DMAs
 *
 */
static void
pkt_dma_memory_to_host(struct mu_buf_dma_desc *mu_buf_dma_desc,
                       int dma_start_offset, int dma_size, int token)
{
    uint64_32_t cpp_addr;
    uint64_32_t pcie_addr;

    cpp_addr.uint32_lo = ((mu_buf_dma_desc->mu_base_s8 << 8) +
                          dma_start_offset);
    cpp_addr.uint32_hi = (mu_buf_dma_desc->mu_base_s8 >> 24);
    pcie_addr.uint32_lo = (mu_buf_dma_desc->pcie_base_low +
                    dma_start_offset);
    pcie_addr.uint32_hi = mu_buf_dma_desc->pcie_base_high;
    pcie_dma_buffer(PKT_CAP_PCIE_ISLAND, pcie_addr,
                    cpp_addr, dma_size,
                                 NFP_PCIE_DMA_TOPCI_HI, token, PKT_CAP_PCIE_DMA_CONFIG);
}

/** pkt_dma_slave_get_desc - 20i + 300d
 * 20i + workq add thread + 3 parallel MU read
 *
 * Get a quantum of DMA work from the MU work q (muq_to_host_dma), and
 * get the details from the MU buffer descriptor.
 *
 */
static __intrinsic void
pkt_dma_slave_get_desc(struct mu_buf_dma_desc *mu_buf_dma_desc)
{
    __xread struct mu_buf_to_host_dma_work mu_buf_read;
    uint32_t mu_base_s8;
    int first_packet, last_packet;
    int first_packet_ofs, last_packet_ofs;
    int start_block, end_block;
    int mu_buf_hdr_size, mu_pkt_buf_desc_size;
    SIGNAL sig1, sig2, sig3;
    __xread struct mu_pkt_buf_desc first_pkt_desc, last_pkt_desc;
    __xread struct mu_buf_hdr mu_buf_hdr;

    mem_workq_add_thread(muq_to_host_dma, (void *)&mu_buf_read,
                         sizeof(mu_buf_read));

    mu_buf_dma_desc->first_packet = mu_buf_read.first_packet;
    mu_buf_dma_desc->num_packets  = mu_buf_read.num_packets;
    mu_buf_dma_desc->mu_base_s8   = mu_buf_read.mu_base_s8;
    mu_base_s8 = mu_buf_read.mu_base_s8;
    last_packet = (mu_buf_dma_desc->first_packet +
                   mu_buf_dma_desc->num_packets - 1);

    first_packet_ofs = (offsetof(struct mu_buffer, pkt_desc) +
                        ((mu_buf_dma_desc->first_packet) *
                         sizeof(struct mu_pkt_buf_desc)));
    last_packet_ofs  = (offsetof(struct mu_buffer, pkt_desc) +
                        (last_packet * sizeof(struct mu_pkt_buf_desc)));

    mu_pkt_buf_desc_size = (sizeof(struct mu_pkt_buf_desc) /
                            sizeof(uint64_t));
    mu_buf_hdr_size      = (sizeof(struct mu_buf_hdr) /
                            sizeof(uint64_t));
    mu_base_s8 = mu_buf_read.mu_base_s8;
    __asm {
        mem[read, first_pkt_desc, mu_base_s8, <<8, first_packet_ofs, \
            mu_pkt_buf_desc_size], sig_done[sig1];
        mem[read, last_pkt_desc,  mu_base_s8, <<8, last_packet_ofs, \
            mu_pkt_buf_desc_size], sig_done[sig2];
        mem[read, mu_buf_hdr,     mu_base_s8, <<8, 0, mu_buf_hdr_size],\
            sig_done[sig3];
    }
    wait_for_all(&sig1, &sig2, &sig3);

    mu_buf_dma_desc->pcie_base_low  = mu_buf_hdr.pcie_base_low;
    mu_buf_dma_desc->pcie_base_high = mu_buf_hdr.pcie_base_high;
    mu_buf_dma_desc->first_block = first_pkt_desc.offset;
    mu_buf_dma_desc->end_block   = (last_pkt_desc.offset +
                                    last_pkt_desc.num_blocks);
}

/** packet_capture_dma_to_host_slave - 292i + 5450d (128 64B pkt/10 1.5kB pkt)
 * 20i + 20i + 300d + 8*28i + 8*100d + 8*500d + 1*28i + 1*100d + 1*250d
 * for 16kB transfer = 292i + 5450d
 *
 * At most 64 packets - need to redo a bit of calculation here
 * for 128 64B packets (2us), or for 10 1.5kB packets (3us?)
 *
 * Takes ~5% CPU per thread at full load
 *
 * The DMA threads put themselves on the DMA work queue, and take the
 * data from the DMA master thread.  They issue a DMA (or number of DMAs)
 * to move the packet data, and then they issue a DMA to move the
 * packet descriptor data.  When the DMAs complete they increment the
 * 'dmas complete' in the master thread structure and add themselves
 * back to the DMA work queue.
 */
void
packet_capture_dma_to_host_slave(void)
{
    for(;;) {
        struct mu_buf_dma_desc mu_buf_dma_desc;
        int dma_start_offset, dma_length;
        int mu_base_s8, mu_offset;

        pkt_dma_slave_get_desc(&mu_buf_dma_desc);

        dma_start_offset = (mu_buf_dma_desc.first_block << 6);
        dma_length       = ((mu_buf_dma_desc.end_block << 6) -
                            dma_start_offset);
        pkt_dma_memory_to_host(&mu_buf_dma_desc, dma_start_offset,
                               dma_length, 3);

        dma_start_offset = (offsetof(struct mu_buffer, pkt_desc) +
                            (mu_buf_dma_desc.first_packet *
                             sizeof(struct mu_pkt_buf_desc)));
        dma_length       = (mu_buf_dma_desc.num_packets * 
                            sizeof(struct mu_pkt_buf_desc));
        pkt_dma_memory_to_host(&mu_buf_dma_desc, dma_start_offset,
                               dma_length, 0);

        mu_offset = offsetof(struct mu_buffer, dmas_completed);
        mu_base_s8 = mu_buf_dma_desc.mu_base_s8;
        __asm {
            mem[incr, --, mu_base_s8, <<8, mu_offset]
        }
    }
}

/** dma_master_enqueue_next_pkts_ready
 */
static int
dma_master_enqueue_next_pkts_ready(uint32_t mu_base_s8,
                                   uint32_t first_packet)
{
    int w; /* Word of bitmask corresponding to first_packet */
    int b; /* Bit of bitmask corresponding to first_packet */

    int mu_offset; /* Offset to start of bitmask for first_packet */
    __xread uint32_t bitmask[2]; /* Bitmask from MU buffer */

    int x; /* INVERSE of bits of bitmask (first set is NOT ready) */
    int n; /* Number of consecutive bits set in bitmask starting at
              first_packet */

    struct mu_buf_to_host_dma_work mu_buf_to_host_dma;
    __xwrite struct mu_buf_to_host_dma_work mu_buf_to_host_dma_out;
    SIGNAL sig;

    w = first_packet >> 5;
    b = first_packet & 0x1f;

    mu_offset = offsetof(struct mu_buffer, pkt_bitmask) + (w << 2);
    mem_atomic_read_s8(&bitmask[0], mu_base_s8, mu_offset,
                       sizeof(bitmask));

    /* Is first_packet ready? If not, return 0 packets ready
     */
    x = bitmask[0] >> b;
    if (!(x & 1)) return 0;

    /* At least one packet ready, get 'not ready' mask for next packets
       Note that x will now have at LEAST the top bit set
       So n will be from 0 to 31 (0=> just b is ready).
       And number of packets ready is n+1
     */
    x = ~(x >> 1);
    __asm {
        ffs[n, x];
    }
    n += 1;

    /* If the rest of the packets in bitmask[0] are ready, try
     * bitmask[1] too
     */
    if ((n + b) == 32) {
        x = ~bitmask[1];
        if (x == 0) {
            n += 32;
        } else {
            int n2; /* Number of first bit clear in bitmask[1] */
            __asm {
                ffs[n2, x];
            }
            n += n2;
        }
    }

    /* Build DMA work for 'n' packets from first_packet
     */
    mu_buf_to_host_dma.mu_base_s8   = mu_base_s8;
    mu_buf_to_host_dma.first_packet = first_packet;
    mu_buf_to_host_dma.num_packets  = n;
    mu_buf_to_host_dma_out = mu_buf_to_host_dma;

    mem_workq_add_work(muq_to_host_dma, (void *)&mu_buf_to_host_dma_out,
                       sizeof(mu_buf_to_host_dma));
    return n;
}

/** packet_capture_dma_to_host_master
 *
 * This thread takes control of an MU buffer after it starts to be
 * used, and manages DMA of the whole buffer over time to the host.
 *
 * It starts operating when it receives an MU buffer from the first
 * allocator, using the mu_buf_in_use work queue.
 *
 * It monitors the bit-mask of packets ready in the MU buffer.  It
 * then packages up DMAs for slaves: PCIe base, MU base, first packet,
 * number of packets, and presents them as work to DMA slave
 * threads. When the MU buffer is completed and all the DMA threads
 * complete the MU buffer can be recycled.
 * 
 * It has a 'first packet' which is set to 0 initially. It monitors up
 * to 64 bits from 'first packet' of the bitmask of completed packets.
 * When the bit for 'first packet' is set, it counts the number of
 * consecutive set bits from that one upwards, and adds a DMA of that
 * size (therefore of up to 64 packets). It then moves on 'first_packet'
 *
 * If the bit is not set for 'first packet', the thread reads
 * 'total_packets'; this is set when the receivers complete using the
 * MU buffer. If 'first packet' is equal to 'total_packets' then the
 * buffer is done; if not, sleep and poll the bitmask again.
 *
 * When the buffer is done, the thread waits for all the DMAs to
 * complete and then the MU buffer is recycled
 * 
 */
void
packet_capture_dma_to_host_master(int poll_interval)
{
    for(;;) {
        __xread uint32_t mu_base_s18; /* MU buff addr >>18 from workq */
        uint32_t mu_base_s8;          /* MU buf addr >>8 for CPP cmd */
        __xread struct mu_buf_hdr mu_buf_hdr_in; /* Just for total_packets */
        __xwrite struct mu_buf_hdr mu_buf_hdr_out;
        __xwrite uint32_t mu_base_s8_out; /* MU base for recyle workq */
        int first_packet;  /* First packet to give to next slave */
        int total_packets; /* Total packets in the MU buffer */
        int total_dmas;    /* Total number of slave DMAs */

        /* Get next MU buf to work on
         */
        mem_workq_add_thread(muq_mu_buf_in_use, &mu_base_s18,
                             sizeof(mu_base_s18));

        mu_base_s8 = mu_base_s18 << 10;
        mem_atomic_read_s8(&mu_buf_hdr_in, mu_base_s8, 0,
                           sizeof(uint32_t)*2);
        total_packets = mu_buf_hdr_in.total_packets;

        /* Add slave DMA batches until all of MU buf is batched up
         */
        first_packet = 0;
        for (;;) {
            int num_pkts; /* Number of packets enqueued to slave */
            num_pkts = dma_master_enqueue_next_pkts_ready(mu_base_s8,
                                                          first_packet);
            if (num_pkts == 0) {
                if ((total_packets != 0) &&
                    (first_packet == total_packets))
                    break;

                me_sleep(poll_interval);
                mem_atomic_read_s8(&mu_buf_hdr_in, mu_base_s8, 0,
                                   sizeof(uint32_t)*2);
            }
            else {
                total_dmas += 1;
                first_packet += num_pkts;
            }
        }

        /* Poll the 'dmas_completed' element until it matches total_dmas
         */
        for (;;) {
            int mu_offset; /* Offset to dmas_completed in MU buf */
            __xread uint32_t dmas_completed;

            mu_offset = offsetof(struct mu_buffer, dmas_completed);
            mem_atomic_read_s8(&dmas_completed, mu_base_s8, mu_offset,
                               sizeof(dmas_completed));
            if (dmas_completed == total_dmas) break;
            me_sleep(poll_interval);
        }

        /*b Recycle the MU buf
         */
        mu_base_s8_out = mu_base_s8;
        mem_workq_add_work(muq_mu_buf_recycle, &mu_base_s8_out,
                           sizeof(mu_base_s8_out));
    }
}

/** pkt_add_mu_buf_desc - 20i + 200d
 * 20 inst + 4 parallel MU bulk write + MU add work
 *
 * Set up an MU buffer, zeroing required bitmask data, and add it to
 * the mu_buf_alloc workq
 *
 */
static __intrinsic void
pkt_add_mu_buf_desc(uint32_t mu_base_s8, int buf_seq,
                    struct pcie_buf_desc *pcie_buf_desc)
{
    int c_128;       /* =128, in a register as required by assembler */
    SIGNAL sig1, sig2, sig3, sig4;         /* Completion signals */
    __xwrite struct mu_buf_hdr mu_buf_hdr; /* MU buffer header data */
    __xwrite uint64_t zeros[8];            /* Zeros to clear bitmask */

    struct mu_buf_desc mu_buf_desc; /* MU buffer descriptor for workq */
    __xwrite struct mu_buf_desc mu_buf_desc_out; /* Xfer of MU buffer
                                                    descriptor */

    mu_buf_hdr.buf_seq = buf_seq;
    mu_buf_hdr.total_packets = 0;
    mu_buf_hdr.pcie_base_low = pcie_buf_desc->pcie_base_low;
    mu_buf_hdr.pcie_base_high = pcie_buf_desc->pcie_base_high;

    zeros[0] = 0;
    zeros[1] = 0;
    zeros[2] = 0;
    zeros[3] = 0;
    zeros[4] = 0;
    zeros[5] = 0;
    zeros[6] = 0;
    zeros[7] = 0;
    c_128 = 128;
    __asm {
        mem[write32,mu_buf_hdr,mu_base_s8,<<8, 0, 4], sig_done[sig1];
        mem[write,zeros,mu_base_s8,<<8, 16, 6],  sig_done[sig2];
        mem[write,zeros,mu_base_s8,<<8, 64, 8],  sig_done[sig3];
        mem[write,zeros,mu_base_s8,<<8, c_128, 8], sig_done[sig4];
    }
    wait_for_all(&sig1, &sig2, &sig3, &sig4);

    mu_buf_desc.__raw = 0;
    mu_buf_desc.offset = 0;
    mu_buf_desc.number = 0;
    mu_buf_desc.mu_base_s18 = mu_base_s8>>10;
    mu_buf_desc_out = mu_buf_desc;
    mem_workq_add_work(muq_mu_buf_alloc, (void *)&mu_buf_desc_out,
                       sizeof(mu_buf_desc));
}

/** host_get_buf
 *
 * Get a buffer from the host ring. Must run on host PCIe island.
 *
 * @param host_data      Host data read at init time
 * @param pcie_buf_desc  PCIe buffer descriptor to fill
 * @param poll_interval  Time interval to poll
 *
 */
static void
host_get_buf(struct host_data *host_data,
             struct pcie_buf_desc *pcie_buf_desc,
             int poll_interval)
{
    uint32_t addr; /* Address in CLS of host data / ring */
    uint32_t ofs;  /* Offset in to ring of 'rptr' entry */
    __xread struct pcie_buf_desc pcie_buf_desc_in;

    addr = host_data->cls_host_shared_data;
    if (host_data->wptr == host_data->rptr) {
        __xread uint32_t wptr; /* Xfer to read CLS wptr */
        for (;;) {
            cls_read(&wptr, (__cls void *)addr, 0,
                     sizeof(uint32_t));
            if (wptr != host_data->rptr) break;
            me_sleep(poll_interval);
        }
        host_data->wptr = wptr;
    }
    addr = host_data->cls_host_ring_base;
    ofs = host_data->rptr & host_data->cls_host_ring_item_mask;
    ofs = ofs << 3;
    cls_read(&pcie_buf_desc_in, (__cls void *)addr, ofs, 
                 sizeof(uint64_t));
    host_data->rptr++;
    *pcie_buf_desc = pcie_buf_desc_in;
}

/** packet_capture_mu_buffer_recycler - ?
 *
 * Get PCIe buffers from the host, collect with recycled buffers, then
 * add them to the muq_mu_buf_alloc workq.
 * 
 * If an MU buffer lasts for 1k 64B packets then this needs to run every
 * ~8k cycles.
 *
 * If an MU buffer lasts for 256kB=2Mbits of data then this needs to
 * run every ~20k cycles.
 *
 * The max utilization of the ME is <1%.
 */
void
packet_capture_mu_buffer_recycler(int poll_interval)
{
    struct host_data host_data; /* Host data cached in shared registers */
    int buf_seq; /* Monotonically increasing buffer sequence number */

    host_data.cls_host_shared_data = __link_sym("pcap_cls_host_shared_data");
    host_data.cls_host_ring_base   = __link_sym("pcap_cls_host_ring_base");
    host_data.cls_host_ring_item_mask = (PCAP_HOST_CLS_RING_SIZE>>2)-1;
    host_data.rptr = 0;
    host_data.wptr = 0;

    buf_seq = 0;
    for (;;) {
        __xread uint32_t mu_base_s8;
        struct pcie_buf_desc pcie_buf_desc;

        host_get_buf(&host_data, &pcie_buf_desc, poll_interval);

        /* Could load balance MUs here a bit? Rotate round different
         * recycle rings?*/
        mem_workq_add_thread(muq_mu_buf_recycle, &mu_base_s8,
                             sizeof(mu_base_s8));
        pkt_add_mu_buf_desc(mu_base_s8, buf_seq, &pcie_buf_desc);
        buf_seq++;
    }
}

/** packet_capture_fill_mu_buffer_list
 *
 * Fill the MU buffer list with 'num_buf' 256kB buffers starting at
 * given base
 *
 * @param  mu_base_s8   MU buffer base address >> 8
 * @param  num_buf      Number of 256kB buffers to fill with
 *
 */
void
packet_capture_fill_mu_buffer_list(uint32_t mu_base_s8, int num_buf)
{
    for (;num_buf>0;num_buf--) {
        __xwrite uint32_t mu_base_s8_out;
        mu_base_s8_out = mu_base_s8;
        mem_workq_add_work(muq_mu_buf_recycle, &mu_base_s8_out,
                           sizeof(mu_base_s8));
        mu_base_s8 += ((1<<18)>>8);
    }
}

/** packet_capture_init_pkt_rx_dma
 *
 * Perform initialization for the packet rx DMA threads
 *
 * Gets queue configuration required by the threads
 *
 */
__intrinsic void packet_capture_init_pkt_rx_dma(void)
{
    muq_mu_buf_in_use  = MU_QUEUE_CONFIG_GET(QDEF_MU_BUF_IN_USE);
    muq_mu_buf_alloc   = MU_QUEUE_CONFIG_GET(QDEF_MU_BUF_ALLOC);
}

/** packet_capture_init_mu_buffer_recycler
 *
 * Perform initialization for the MU buffer recycler
 *
 * Writes configuration of the MU_BUF_ALLOC queue
 * Gets queue configuration required by the threads
 *
 */
__intrinsic void packet_capture_init_mu_buffer_recycler(void)
{
    muq_mu_buf_alloc   = MU_QUEUE_CONFIG_WRITE(QDEF_MU_BUF_ALLOC);
    muq_mu_buf_recycle = MU_QUEUE_CONFIG_GET(QDEF_MU_BUF_RECYCLE);
    muq_to_host_dma    = MU_QUEUE_CONFIG_GET(QDEF_TO_HOST_DMA);
}

/** packet_capture_init_dma_to_host_master
 *
 * Perform initialization for the DMA to host master
 *
 * Writes configuration of the MU_BUF_RECYCLE, TO_HOST_DMA and
 * MU_BUF_IN_USE queues
 *
 */
__intrinsic void packet_capture_init_dma_to_host_master(void)
{
    muq_mu_buf_recycle = MU_QUEUE_CONFIG_WRITE(QDEF_MU_BUF_RECYCLE);
    muq_to_host_dma    = MU_QUEUE_CONFIG_WRITE(QDEF_TO_HOST_DMA);
    muq_mu_buf_in_use  = MU_QUEUE_CONFIG_WRITE(QDEF_MU_BUF_IN_USE);
}

/** packet_capture_init_dma_to_host_slave
 *
 * Perform initialization for the DMA to host slave
 *
 * Gets TO_HOST_DMA queue configuration for the thread
 *
 */
__intrinsic void packet_capture_init_dma_to_host_slave(void)
{
    muq_to_host_dma    = MU_QUEUE_CONFIG_GET(QDEF_TO_HOST_DMA);
}

