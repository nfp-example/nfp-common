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
 * @file        pktgen_lib.c
 * @brief       A packet generator library for pktgen application
 *
 * This is a library to support a packet generator using a host x86
 * system to supply scripted packet generation
 * 
 */
/** Outline of code
 *
 * The host delivers basic packets into the MU at 2kB+64 alignments,
 * or if the packets are <192B to a 256B+64B alignment.
 * 
 * The host delivers a set of 'flow' descriptors, or scripts. These
 * are a set of scripts that are combined with a basic packet to form
 * a packet for transmission.
 * 
 * The host also delivers a schedule, or stream of batches of
 * flow-packets, to transmit. Each batch is 8 flow-packet
 * entries. Each flow-packet entry in a batch is a packet
 * address+length, a script offset, and a time (in ns) at which to
 * transmit the packet.
 *
 * Each flow-packet entry is 16B.
 *     uint32_t     tx_time_lo;
 *     unsigned int tx_time_hi:8;
 *     uint32_t     mu_base_s8;
 *     unsigned int script_ofs:24;
 *     unsigned int length:16;
 *     unsigned int flags:16;
 *
 * The stream is executed by a master thread that distributes a basic
 * time, a batch sequence number and a tx sequence order number to a
 * set of batch distributor threads. The master thread keeps track of
 * how many packets have completed processing using global memory
 * locations (one per batch, incremented upon completion of tx by
 * packets processing by TX slave transmitters). The master thread can
 * then hold off adding work to the batch queues to stop the
 * downstream work queues from overflowing.
 *
 * The batch distributor threads take the batch sequence number, base
 * time and tx sequence order and read in the 8 flow-packet entries
 * for their batch. They add each entry as TX slave work items to the
 * work queue for each batch (each batch has a work queue). In this
 * way the work queue for a batch has at most 1/8th of the packet rate
 * added to it per second (limiting the MU queue engine stress on that
 * QA to <10Mops for 40GbE).
 *
 * A work item for the TX slave transmitter added by the batch
 * distributors is:
 *
 *     uint32_t     tx_time_lo;
 *     unsigned int tx_time_hi:8;
 *     unsigned int script_ofs:24;
 *     uint32_t     mu_base_s8;
 *     unsigned int length:16;
 *     unsigned int tx_seq:16;
 * A length of 0 implies no packet (and tx_seq is unused)
 *
 * The TX slave transmitter performs the following steps:
 *
 * 1) Wait for permission to progress within the batch (in range say 1024 'in the future' tx seq)
 * 2) Alloc
 * 3) Start DMA from MU to CTM (first <192B)
 * 4) Start script read
 * 5) Write header
 * 6) Wait for DMA completion
 * 7) Overwrite DMA data
 * 8) Wait time
 * 9) Tx sequence
 * 10) Increment global batch 'ready' indicator
 *
 * Each batch has a 'total released' claimant globally
 * As a worker on a batch item, you can only start if the work item's transmit sequence number is < 'last transmitted' + FIXED_N
 * When the NBI BLM is done transmitting the buffer recycling can update 'last transmitted'
 *
 * CTM credit has to be handled per-CTM in hardware. 256B is used for
 * each transmit packet, so 1k packets are supported max (probably go
 * for 512 per CTM in fact, to provide space in the CTM for other
 * stuff in the future? With the batch mechanism there is already
 * 'fairness' to a great degree. So credit can be global for a CTM.
 *
 * For 40GbE 64B packets there are 60Mpps, or 7.5Mpps for each of the eight 'batch'es.
 * 
 * The CPP latency budget for a thread loop (64B packet) can then be:
 * MU work q (250 cycles)
 * CTM credit (150 cycles)
 * Packet alloc (100 cycles)
 * CTM DMA (500 cycles)
 * Read script (250 cycles)
 * Write header (100 cycles)
 * Overwrite DMA data (150 cycles, with readback to ensure order)
 * Packet tx (0 cycles, no signal)
 * Total of 1,500 cycles of latency
 *
 * For processing
 * Add to work q (10 cycles)
 * CTM credit (30 cycles)
 * Alloc (20 cycles)
 * CTM DMA (30 cycles)
 * Read script (10 cycles)
 * Write header (20 cycles)
 * Wait for DMA completion (10 cycles)
 * Overwrite DMA data (30 cycles)
 * Wait time (20 cycles)
 * Tx sequence (20 cycles)
 * Total 200 cycles
 *
 * With this utilization we have ~200 ME instruction cycles out of
 * ~1700 total cycles, with 8 threads. So an ME would achieve 8
 * packets (using 8 threads) every 1700 cycles, or 4.7Mpps Hence two
 * MEs could accomplish a batch performance target of 7.5Mpps at 80%
 * utilization
 *
 * Note that the CTM DMA latency is likely to be a bottleneck, as
 * threads will not be able to all overlap.  So peak ME utilization
 * may only be 50% cycles?
 *
 * Also note that the above includes a simple 'null' script concept
 * (presumably script is a simple overwrite of the header). If an
 * interpreted script is used then more ME cycles will be consumed.
 *
 * This points to a need for more than two MEs per batch, perhaps as
 * many as four MEs. This would consume most of three ME islands for
 * all 8 batches. Note then that a batch has 32 packets being
 * processed simultaneously.
 *
 *
 * A script needs a type (fixed at present) and data fields
 *
 * For a TCP flow we need to change dest Eth, dest IP, and TCP ports
 * Possibly we need to change D/S Eth (12 bytes), S/D IP (8 bytes), TCP ports (4 bytes)
 * One way to do this is two mem writes, with 24 bytes of data
 * Further one may want to add to the IPv4 checksum (assuming the checksum ignores the S/D IP).
 * Also the TCP checksum.
 * The checksum updates require a read, add (16 bit), carry in, and write
 *
 * We probably also need an 'encapsulate in VXLAN' mode, or option to
 * achieve this. This would require a new Eth D/S, some fixed IP header fields, an IP length write, UDP SP, UDP length, VXLAN header (8B).
 * This is a prepend of 44B with two length field writes.
 *
 * So a script should be 64B of data
 */
/** Includes
 */
#include <stdint.h>
#include <nfp/me.h>
#include <nfp/mem.h>
#include <nfp/cls.h>
#include <nfp/pcie.h>
#include <nfp.h>
#include <nfp_override.h>
#include "pktgen_lib.h"
#include "firmware/pktgen.h"

/** Defines
 */
#define MAX_BATCH_ITEMS_IN_PROCESSING (32)

/** Memory declarations
 */
_alloc_mem("pktgen_cls_host_res cls island 64 64")
_alloc_mem("pktgen_cls_ring_res cls island " PKTGEN_CLS_RING_SIZE__STR " " PKTGEN_CLS_RING_SIZE__STR)
_declare_resource("pktgen_cls_host island 64 pktgen_cls_host_res")
_declare_resource("pktgen_cls_ring island " PKTGEN_CLS_RING_SIZE__STR " pktgen_cls_ring_res")
#define ALLOC_PKTGEN_HOST() __alloc_resource("pktgen_cls_host pktgen_cls_host island 64")
#define ALLOC_PKTGEN_RING() __alloc_resource("pktgen_cls_ring pktgen_cls_ring island " PKTGEN_CLS_RING_SIZE__STR)

/** Queue descriptors and allocations
 */
/* Batch work queue is from pktgen_master to batch_distributor */
/* Currently 1k words = 256 entries = 4k packets*/
#define QDEF_BATCH_WORK mu_workq_batch_work,10,8,emem

/* Batch descriptor queues are from batch_distributor to tx_slaves */
/* Currently 1k words = 256 entries = 4k packets*/
#define QDEF_BATCH_DESC_0 mu_workq_batch_desc_0,10,16,emem
#define QDEF_BATCH_DESC_1 mu_workq_batch_desc_1,10,17,emem
#define QDEF_BATCH_DESC_2 mu_workq_batch_desc_2,10,18,emem
#define QDEF_BATCH_DESC_3 mu_workq_batch_desc_3,10,19,emem
#define QDEF_BATCH_DESC_4 mu_workq_batch_desc_4,10,20,emem
#define QDEF_BATCH_DESC_5 mu_workq_batch_desc_5,10,21,emem
#define QDEF_BATCH_DESC_6 mu_workq_batch_desc_6,10,22,emem
#define QDEF_BATCH_DESC_7 mu_workq_batch_desc_7,10,23,emem

MU_QUEUE_ALLOC(QDEF_BATCH_WORK);
MU_QUEUE_ALLOC(QDEF_BATCH_DESC_0);
MU_QUEUE_ALLOC(QDEF_BATCH_DESC_1);
MU_QUEUE_ALLOC(QDEF_BATCH_DESC_2);
MU_QUEUE_ALLOC(QDEF_BATCH_DESC_3);
MU_QUEUE_ALLOC(QDEF_BATCH_DESC_4);
MU_QUEUE_ALLOC(QDEF_BATCH_DESC_5);
MU_QUEUE_ALLOC(QDEF_BATCH_DESC_6);
MU_QUEUE_ALLOC(QDEF_BATCH_DESC_7);

/** struct host_data
 */
struct host_data {
    uint32_t cls_ring_base;
    uint32_t cls_ring_item_mask;
    uint32_t cls_host_shared_data;
    uint32_t wptr;
    uint32_t rptr;
};

/** struct tx_pkt_work
 */
struct tx_pkt_work {
    uint32_t     tx_time_lo;    /* Not sure what units... */
    unsigned int tx_time_hi:8;  /* Top 8 bits */
    unsigned int script_ofs:24; /* Offset to script from script base */
    unsigned int mu_base_s8;    /* 256B aligned packet start */
    unsigned int length:16;     /* Length of the packet (needed to DMA it) */
    unsigned int tx_seq:16;     /* for reorder pool and to stop processing too far into the future */
};

/** struct batch_work
 */
struct batch_work {
    union {
        struct {
            uint32_t     tx_time_lo;   /* Not sure what units... */
            unsigned int tx_time_hi:8; /* Top 8 bits */
            unsigned int num_valid_pkts:8;        /* Pad 8 bits */
            unsigned int tx_seq:16;    /* Tx sequence for first of 8 entries */
            uint32_t     mu_base_s8;   /* 256B aligned flow script start */
            uint32_t     work_ofs;     /* Offset to script from script base */
        };
        uint32_t __raw[4];
    };
};

/** struct tx_seq
 */
struct tx_seq {
    uint32_t last_transmitted;
};

/** struct batch_desc
 */
struct batch_desc {
    uint32_t muq; /* MU queue handle for the batch workq */
    uint32_t override;
    __mem struct tx_seq *tx_seq;
    uint32_t seq_base_s8;
    uint32_t seq_ofs;
};

/** struct ctm_pkt_desc
 */
struct ctm_pkt_desc {
    uint32_t pkt_num;
    uint32_t pkt_addr;
    uint32_t mod_script_offset;
};

/** struct script
 */
struct script {
    uint32_t data[16];
};

/** struct script_finish
 */
struct script_finish {
    uint32_t data[4];
};

/** Statics
 */
__declspec(shared) static struct batch_desc batch_desc;
__declspec(shared) uint32_t batch_desc_muq_array[8];
__declspec(shared) static int last_seq_transmitted;
__declspec(shared) static int poll_interval;
__declspec(shared) uint32_t mu_script_base_s8;
__declspec(shared) uint32_t batch_work_muq;

/** tx_slave_get_pkt_in_batch
 * 10i + 150
 */
void
tx_slave_get_pkt_in_batch(struct tx_pkt_work *tx_pkt_work)
{
    __xread  struct tx_pkt_work tx_pkt_work_in;

    mem_workq_add_thread(batch_desc.muq, (void *)&tx_pkt_work_in,
                         sizeof(tx_pkt_work_in));
    *tx_pkt_work = tx_pkt_work_in;
}

/** tx_slave_wait_for_tx_seq
 * 5i * 90% / 15i+150 * 10% / poll if way ahead
 *
 * = 6i+15
 */
void
tx_slave_wait_for_tx_seq(struct tx_pkt_work *tx_pkt_work)
{

    for (;;) {
        __xread struct tx_seq tx_seq;

        if ((tx_pkt_work->tx_seq - last_seq_transmitted) <
            MAX_BATCH_ITEMS_IN_PROCESSING)
            return;

        mem_atomic_read_s8(&tx_seq, batch_desc.seq_base_s8, batch_desc.seq_ofs, sizeof(tx_seq));
        last_seq_transmitted = tx_seq.last_transmitted;

        if ((tx_pkt_work->tx_seq - last_seq_transmitted) <
            MAX_BATCH_ITEMS_IN_PROCESSING)
            return;

        me_sleep(poll_interval);
    }
}

/** tx_slave_alloc_pkt
 *
 * 8i+300 + poll if required
 *
 * Allocate a 256B CTM packet buffer
 *
 */
#define CTM_ALLOC_256B 0
void
tx_slave_alloc_pkt(struct ctm_pkt_desc *ctm_pkt_desc)
{
    SIGNAL sig;
    uint32_t pkt_num; /* Packet number for assembler command to get
                         packet status from */
    __xread uint32_t pkt_status[2]; /* Packet status, including CTM
                                       address of packet */

    for (;;) {
        int me_credit_bucket = 0;
        __xread uint32_t ctm_pkt;
       __asm {
            mem[packet_alloc_poll, ctm_pkt, me_credit_bucket, 0,\
                CTM_ALLOC_256B], ctx_swap[sig];
        }
        if (ctm_pkt!=0xffffffff) {
            /* ctm_pkt[11;9] is pkt credit, [9;0] buf credit
             */
            ctm_pkt_desc->pkt_num = (ctm_pkt >> 20) & 0x1ff;
            break;
        }
        me_sleep(poll_interval);
    }

    pkt_num = ctm_pkt_desc->pkt_num;
    __asm {
        mem[packet_read_packet_status, pkt_status[0], \
            pkt_num, 0, 1],             \
            ctx_swap[sig]
            }
    ctm_pkt_desc->pkt_addr = (pkt_status[0] & 0x3ff) << 8;
}

/** tx_slave_read_script_start
 */
__intrinsic void
tx_slave_read_script_start(struct tx_pkt_work *tx_pkt_work,
                           __xread struct script *script,
                           SIGNAL *sig)
{
    int script_size;
    uint32_t script_ofs;
    uint32_t base_s8;
    script_size = sizeof(*script) / sizeof(uint64_t);
    script_ofs = tx_pkt_work->script_ofs;
    base_s8 = mu_script_base_s8;
    __asm {
        mem[read, *script, base_s8, <<8,\
            script_ofs, script_size], \
            sig_done[*sig];
    }
}

/** tx_slave_dma_pkt_start
 */
__intrinsic void
tx_slave_dma_pkt_start(struct tx_pkt_work *tx_pkt_work,
                       struct ctm_pkt_desc *ctm_pkt_desc,
                       SIGNAL *sig)
{
    int ctm_dma_len; /* #64Bs to DMA -1 (0->64B, 1->128B etc) */
    uint32_t mu_base_lo;
    uint32_t mu_base_hi;
    int len;

    ctm_dma_len = 0;
    if (tx_pkt_work->length>64)  ctm_dma_len=1;
    if (tx_pkt_work->length>128) ctm_dma_len=2;

    mu_base_lo = tx_pkt_work->mu_base_s8 << 8;
    mu_base_hi = tx_pkt_work->mu_base_s8 >> 24;
    //bm = mu_base_hi;
    //dm/ref = ctm_pkt_desc->pkt_addr >> 3;
    len = ctm_dma_len;
    __asm {
        mem[pe_dma_from_memory_buffer, --, mu_base_lo, 64, 1], \
            indirect_ref, sig_done[*sig];
    }
}

/** tx_slave_pkt_tx
 *
 * 10i
 */
void
tx_slave_pkt_tx(struct tx_pkt_work *tx_pkt_work,
                struct ctm_pkt_desc *ctm_pkt_desc)
{
    /* island,dm,sm - seq#, bm[5;0]=sqr#
     */
    uint32_t sequence_info = 0; /* unordered */
    uint32_t override;
    uint32_t pkt_num_s16;
    uint32_t tx_pkt_length; /* Actual TX packet length */

    pkt_num_s16 = ctm_pkt_desc->pkt_num << 16;
    tx_pkt_length = tx_pkt_work->length; /* Length + mod script... */

    local_csr_write(local_csr_cmd_indirect_ref0, sequence_info );
    override  = batch_desc.override;
    override |= (((ctm_pkt_desc->mod_script_offset / 8) - 1) << 8);
    __asm {
        alu[--, --, b, override];
        mem[packet_complete_unicast, --, pkt_num_s16, tx_pkt_length], \
            indirect_ref;
    }
}

/** tx_slave_script_start
 * Start the script, and keep anything for the finish in script_finish
 * After this call 'script' will be disposed of
 */
void
tx_slave_script_start(struct ctm_pkt_desc *ctm_pkt_desc,
                      __xread struct script *script,
                      struct script_finish *script_finish)
{
}

/** tx_slave_script_finish
 * Start the script, and keep anything for the finish in script_finish
 */
void
tx_slave_script_finish(struct ctm_pkt_desc *ctm_pkt_desc,
                      struct script_finish *script_finish)
{
}

/** tx_slave_wait_for_tx_time
 * Should wait until the transmit time required
 */
void
tx_slave_wait_for_tx_time(struct tx_pkt_work *tx_pkt_work)
{
}

/** pktgen_tx_slave
 * 10i+150 + 6i+15 + ?i + 8i+300 + ?i + 4i+500 + ?i+150 + wait + 10i
 *
 * =  34i + wait + read script start + write CTM pt hdr + overwrite DMA data
 *
 * + 1015 + wait
 */
void
pktgen_tx_slave(void)
{
    int nbi=8;

    batch_desc.override = ( (1<<0) | (1<<1) | (1<<3) | (1<<6) | (1<<7) );
    //batch_desc.override |= (nbi<<12)<<16; /* NBI is 2 bits */
    batch_desc.override |= (0<<12)<<16; /* NBI is 2 bits */
    //batch_desc.override |= batch_desc.queue << 16; /* For the packet complete */
    batch_desc.override |= 0 << 16; /* queue 0 */
    for (;;) {
        struct tx_pkt_work tx_pkt_work;
        struct ctm_pkt_desc ctm_pkt_desc;
        __xread struct script script;
        struct script_finish script_finish;
        SIGNAL dma_sig;
        SIGNAL script_sig;

        tx_slave_get_pkt_in_batch(&tx_pkt_work);
        if (tx_pkt_work.mu_base_s8 == 0) {
            continue;
        }
    local_csr_write(local_csr_mailbox0, tx_pkt_work.mu_base_s8);
    local_csr_write(local_csr_mailbox1, tx_pkt_work.tx_seq);
    local_csr_write(local_csr_mailbox2, tx_pkt_work.length);
    local_csr_write(local_csr_mailbox3, tx_pkt_work.script_ofs);
        __asm {ctx_arb[bpt]}
        tx_slave_wait_for_tx_seq(&tx_pkt_work);
        tx_slave_read_script_start(&tx_pkt_work, &script, &script_sig);
        tx_slave_alloc_pkt(&ctm_pkt_desc);
        tx_slave_dma_pkt_start(&tx_pkt_work, &ctm_pkt_desc, &dma_sig);
        wait_for_all(&script_sig);
        tx_slave_script_start(&ctm_pkt_desc, &script, &script_finish);
        wait_for_all(&dma_sig);
        tx_slave_script_finish(&ctm_pkt_desc, &script_finish);
        tx_slave_wait_for_tx_time(&tx_pkt_work);
        tx_slave_pkt_tx(&tx_pkt_work, &ctm_pkt_desc);
    }
}

/** batch_dist_add_pkt_to_batch
 */
__intrinsic void
batch_dist_add_pkt_to_batch(struct batch_work *batch_work,
                            __xread struct pktgen_sched_entry *sched_entry,
                            __xwrite struct tx_pkt_work *tx_pkt_work_out,
                            int i,
                            SIGNAL *sig)
{
    uint32_t tx_seq;
    struct tx_pkt_work tx_pkt_work;

    tx_seq = batch_work->tx_seq;
    tx_pkt_work.tx_time_lo = batch_work->tx_time_lo + sched_entry->tx_time_lo;
    tx_pkt_work.tx_time_hi = batch_work->tx_time_hi + sched_entry->tx_time_hi; /* carry */
    tx_pkt_work.mu_base_s8 = sched_entry->mu_base_s8;
    tx_pkt_work.script_ofs = sched_entry->script_ofs;
    tx_pkt_work.length     = sched_entry->length;
    tx_pkt_work.tx_seq     = tx_seq+i;
    if (i >= batch_work->num_valid_pkts) {
        tx_pkt_work.mu_base_s8 = 0;
    }
    tx_pkt_work_out[i] = tx_pkt_work;
    mem_workq_add_work_async(batch_desc_muq_array[i], (void *)tx_pkt_work_out,
                             sizeof(tx_pkt_work), sig);
}

/** batch_dist_distribute_sched_entries
 */
__intrinsic
void batch_dist_distribute_sched_entries(struct batch_work *batch_work,
                                        __xread struct pktgen_sched_entry *sched_entries)
{
    __xwrite struct tx_pkt_work tx_pkt_work[8];
    SIGNAL sig0, sig1, sig2, sig3, sig4, sig5, sig6, sig7;
    batch_dist_add_pkt_to_batch(batch_work, &sched_entries[0], &tx_pkt_work[0], 0, &sig0 );
    batch_dist_add_pkt_to_batch(batch_work, &sched_entries[1], &tx_pkt_work[1], 1, &sig1 );
    batch_dist_add_pkt_to_batch(batch_work, &sched_entries[2], &tx_pkt_work[2], 2, &sig2 );
    batch_dist_add_pkt_to_batch(batch_work, &sched_entries[3], &tx_pkt_work[3], 3, &sig3 );
    batch_dist_add_pkt_to_batch(batch_work, &sched_entries[4], &tx_pkt_work[4], 4, &sig4 );
    batch_dist_add_pkt_to_batch(batch_work, &sched_entries[5], &tx_pkt_work[5], 5, &sig5 );
    batch_dist_add_pkt_to_batch(batch_work, &sched_entries[6], &tx_pkt_work[6], 6, &sig6 );
    batch_dist_add_pkt_to_batch(batch_work, &sched_entries[7], &tx_pkt_work[7], 7, &sig7 );
    wait_for_all(&sig0, &sig1, &sig2, &sig3, &sig4, &sig5, &sig6, &sig7);
}

/** batch_dist_get_batch_work
 */
__intrinsic void
batch_dist_get_batch_work(struct batch_work *batch_work)
{
    __xread struct batch_work batch_work_in;
    mem_workq_add_thread(batch_work_muq, (void *)&batch_work_in,
                       sizeof(batch_work));
    *batch_work = batch_work_in;
}

/** batch_dist_get_sched_entries
 */
__intrinsic void
batch_dist_get_sched_entries(struct batch_work *batch_work,
                            __xread struct pktgen_sched_entry sched_entries[8])
{
    mem_read64_s8( sched_entries, batch_work->mu_base_s8,
                   batch_work->work_ofs, sizeof(sched_entries));
}

/** pktgen_batch_distributor
 *
 * A master consumes batch work
 *
 * It generates 8 packets for the tx slaves (one per batch).
 *
 * The work in is 16B:
 *
 * a 32-bit packet-flow entry offset
 * a 16-bit batch sequence (same for all 8 batches)
 * a 16-bit transmit sequence (for the Tx orderer)
 * a 4-bit number of valid packets
 * a 40-bit base time.
 *
 * It reads eight packet-flow entries at a time (128B)
 *
 */
void
pktgen_batch_distributor(void)
{
    for (;;) {
        struct batch_work batch_work;
        struct ctm_pkt_desc ctm_pkt_desc;
        __xread struct pktgen_sched_entry sched_entries[8];

        batch_dist_get_batch_work(&batch_work);
        batch_dist_get_sched_entries(&batch_work, sched_entries);
        batch_dist_distribute_sched_entries(&batch_work, sched_entries);
    }
}

/** tx_master_add_batch_work
 */
__intrinsic void
tx_master_add_batch_work(struct batch_work *batch_work)
{
    __xwrite struct batch_work batch_work_out;
    batch_work_out = *batch_work;
    mem_workq_add_work(batch_work_muq, (void *)&batch_work_out,
                       sizeof(*batch_work));
}

/** tx_master_distribute_schedule - to do - manage backup
 * The batches are credit-managed
 *
 * Probably need to do a number of batch_works at the same time to get
 * the rate up, since each batch work is 8 packets
 *
 * A burst of 4 at one time would be good.
 *
 */
static void
tx_master_distribute_schedule(uint64_t base_time,
                              int total_pkts,
                              uint32_t mu_base_s8,
                              uint32_t *tx_seq)
{
    struct batch_work batch_work;
    int num_batches;

    num_batches = (total_pkts + 7) >> 3;

    batch_work.tx_time_lo = (uint32_t) base_time;
    batch_work.tx_time_hi = ((uint32_t) (base_time >> 32)) & 0xff;
    batch_work.mu_base_s8 = mu_base_s8;
    batch_work.work_ofs = 64;
    batch_work.tx_seq = *tx_seq;
    batch_work.num_valid_pkts = 8;
    *tx_seq = *tx_seq + total_pkts;

    while (num_batches>0) {
        //if (too backed up) {
        //    me_sleep(poll_interval);
        //    read_backup();
        //    continue;
        //}
        if (total_pkts<8) {
            batch_work.num_valid_pkts = total_pkts;
        }
        tx_master_add_batch_work(&batch_work);
        batch_work.work_ofs += 128;
        batch_work.tx_seq += 8;
        total_pkts -= 8;
        num_batches -= 1;
    __asm {ctx_arb[kill]}
    }
}

/** host_get_cmd
 *
 * Get a command from the host
 *
 * @param host_data   Host data read at init time
 * @param host_cmd    Host command from the host
 *
 */
static void
host_get_cmd(struct host_data *host_data,
             __xread struct pktgen_host_cmd *host_cmd)
{
    uint32_t addr; /* Address in CLS of host data / ring */
    uint32_t ofs;  /* Offset in to ring of 'rptr' entry */

    addr = host_data->cls_host_shared_data;
    if (host_data->wptr == host_data->rptr) {
        __xread uint32_t wptr; /* Xfer to read CLS wptr */
        for (;;) {
            cls_read(&wptr,
                     (__cls void *)addr,
                     offsetof(struct pktgen_cls_host,wptr),
                     sizeof(uint32_t));
            if (wptr != host_data->rptr) break;
            me_sleep(poll_interval);
        }
        host_data->wptr = wptr;
    }
    addr = host_data->cls_ring_base;
    ofs = host_data->rptr & host_data->cls_ring_item_mask;
    ofs = ofs << 4;
    cls_read(host_cmd, (__cls void *)addr, ofs, 
                 sizeof(host_cmd));
    host_data->rptr++;
}

/** host_ack_cmd
 *
 * Acknowledge a command from the host
 *
 * @param host_data   Host data read at init time
 * @param host_cmd    Host command from the host
 *
 */
static void
host_ack_cmd(struct host_data *host_data,
             __xread struct pktgen_host_cmd *host_cmd)
{
    uint32_t addr; /* Address in CLS of host data / ring */
    __xwrite uint32_t data_out;

    addr = host_data->cls_host_shared_data;
    data_out = host_data->rptr;
    cls_write(&data_out, (__cls void *)addr, offsetof(struct pktgen_cls_host, rptr),
              sizeof(data_out));
    data_out = host_cmd->ack_cmd.data;
    cls_write(&data_out, (__cls void *)addr, offsetof(struct pktgen_cls_host, ack_data),
              sizeof(data_out));
}

/** pktgen_master
 *
 * The master monitors the CLS and uses it to indicate that data is ready
 *
 * MU base address for script (do not know how to distribute yet)
 *
 * When started it distributes the work over the batch work queues
 *
 */
void
pktgen_master(void)
{
    struct host_data host_data; /* Host data cached in shared registers */
    int buf_seq; /* Monotonically increasing buffer sequence number */
    int tx_seq;

    host_data.cls_host_shared_data = ALLOC_PKTGEN_HOST();
    host_data.cls_ring_base = ALLOC_PKTGEN_RING();
    host_data.cls_ring_item_mask = (PKTGEN_CLS_RING_SIZE >> 4) - 1;
    host_data.wptr = 0;
    host_data.rptr = 0;

    tx_seq = 0;
    for (;;) {
        __xread uint32_t mu_base_s8;
        __xread struct pktgen_host_cmd host_cmd;

        host_get_cmd(&host_data, &host_cmd);
        if (host_cmd.all_cmds.cmd_type == PKTGEN_HOST_CMD_PKT) {
            uint64_t base_time;
            int total_pkts;
            uint32_t mu_base_s8;
            uint64_32_t time_now;
            time_now = me_time64();
            base_time = time_now.uint64 + host_cmd.pkt_cmd.base_delay;
            mu_base_s8 = host_cmd.pkt_cmd.mu_base_s8;
            total_pkts = host_cmd.pkt_cmd.total_pkts;
            tx_master_distribute_schedule(base_time, total_pkts,
                                          mu_base_s8, &tx_seq);
        } else if (host_cmd.all_cmds.cmd_type == PKTGEN_HOST_CMD_DMA) {
            uint64_32_t cpp_addr;
            uint64_32_t pcie_addr;
            int length;

            cpp_addr.uint32_lo = host_cmd.dma_cmd.mu_base_s8 << 8;
            cpp_addr.uint32_hi = host_cmd.dma_cmd.mu_base_s8 >> 24;
            pcie_addr.uint32_lo = host_cmd.dma_cmd.pcie_base_low;
            pcie_addr.uint32_hi = host_cmd.dma_cmd.pcie_base_high;
            length = host_cmd.dma_cmd.length;

            pcie_dma_buffer(0 /*PCIE_ISLAND*/, pcie_addr, cpp_addr, length,
                            NFP_PCIE_DMA_FROMPCI_HI, 0 /*token*/, PKTGEN_PCIE_DMA_CFG );
        } else if (host_cmd.all_cmds.cmd_type == PKTGEN_HOST_CMD_ACK) {
            host_ack_cmd(&host_data, &host_cmd);
        }
    }
}

/** pktgen_master_init
 */
void pktgen_master_init(void)
{
    uint32_t local_muq;

    batch_work_muq = MU_QUEUE_CONFIG_WRITE(QDEF_BATCH_WORK);
   
    local_muq = MU_QUEUE_CONFIG_WRITE(QDEF_BATCH_DESC_0);
    local_muq = MU_QUEUE_CONFIG_WRITE(QDEF_BATCH_DESC_1);
    local_muq = MU_QUEUE_CONFIG_WRITE(QDEF_BATCH_DESC_2);
    local_muq = MU_QUEUE_CONFIG_WRITE(QDEF_BATCH_DESC_3);
    local_muq = MU_QUEUE_CONFIG_WRITE(QDEF_BATCH_DESC_4);
    local_muq = MU_QUEUE_CONFIG_WRITE(QDEF_BATCH_DESC_5);
    local_muq = MU_QUEUE_CONFIG_WRITE(QDEF_BATCH_DESC_6);
    local_muq = MU_QUEUE_CONFIG_WRITE(QDEF_BATCH_DESC_7);
}

/** pktgen_batch_distributor_init
 */
void pktgen_batch_distributor_init(void)
{
    batch_work_muq          = MU_QUEUE_CONFIG_GET(QDEF_BATCH_WORK);
    batch_desc_muq_array[0] = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_0);
    batch_desc_muq_array[1] = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_1);
    batch_desc_muq_array[2] = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_2);
    batch_desc_muq_array[3] = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_3);
    batch_desc_muq_array[4] = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_4);
    batch_desc_muq_array[5] = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_5);
    batch_desc_muq_array[6] = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_6);
    batch_desc_muq_array[7] = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_7);
}

/** pktgen_tx_slave_init
 */
void pktgen_tx_slave_init(int batch)
{
    mu_script_base_s8 = 0;
    batch_desc.seq_base_s8 = 0; /* MU address of last seq transmitted, to backoff tx slaves - is this necesary if we back off the batch distributor? */
    batch_desc.seq_ofs = 0;
    switch (batch) {
    case 0:
        batch_desc.muq = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_0);
        break;
    case 1:
        batch_desc.muq = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_1);
        break;
    case 2:
        batch_desc.muq = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_2);
        break;
    case 3:
        batch_desc.muq = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_3);
        break;
    case 4:
        batch_desc.muq = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_4);
        break;
    case 5:
        batch_desc.muq = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_5);
        break;
    case 6:
        batch_desc.muq = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_6);
        break;
    case 7:
        batch_desc.muq = MU_QUEUE_CONFIG_GET(QDEF_BATCH_DESC_7);
        break;
    }
}
