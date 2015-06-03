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
/** Proposed outline of code
 *
 * The host delivers basic packets into the MU at 2kB+64 alignments,
 * or if the packets are <192B to a 256B+64B alignment.
 * 
 * The host delivers a set of 'flow' descriptors, or scripts. These
 * are a set of scripts that are combined with a basic packet to form
 * a packet for transmission.
 * 
 * The host also delivers a stream of batches of flow-packets to
 * transmit. Each batch is 8 flow-packet entries. Each flow-packet
 * entry in a batch is a packet address+length, a script offset,
 * and a time (in ns) at which to transmit the packet.
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
 *     uint32_t     mu_base_s8;
 *     unsigned int script_ofs:24;
 *     unsigned int batch_seq:16;
 *     unsigned int tx_seq:16;
 * A script_ofs of 0 implies no packet - just consume the batch_seq
 * (but no tx_seq)
 *
 * The TX slave transmitter performs the following steps:
 *
 * 1) Wait for permission to progress within the batch (in range say 32 outstanding per batch)
 * 2) Wait for CTM credit
 * 3) Alloc
 * 4) Start DMA from MU to CTM (first <192B)
 * 4a) Read script
 * 5) Write header
 * 6) Wait for DMA completion
 * 7) Overwrite DMA data
 * 8) Wait time
 * 9) Tx sequence
 * 10) Increment global batch 'ready' indicator
 *
 * Each batch has a 'total released' claimant globally
 * As a worker on a batch item, you can only start if the work item's batch sequence number is < 'total released' + FIXED_N
 * When you are done with an item (probably item 9 - so that CTM resources are not starved by one batch) you move on the 'total released'.
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
/** struct tx_pkt_work
 */
struct tx_pkt_work {
    uint32_t     tx_time; /* Not sure what units... */
    uint32_t     mu_base_s8;   /* 256B aligned packet start */
    uint32_t     script_ofs;   /* Offset to script from script base */
    unsigned int batch_seq:16; /* batch ordering to stop getting too
                                * far ahead of ourselves */
    unsigned int tx_seq:16;    /* for reorder pool */
};

/** struct batch_desc
 */
struct batch_desc {
    muq;
    __mem struct batch_seq *batch_seq;
};

/** Statics
 */
__declspec(shared) static struct batch_desc batch_desc;
__declspec(shared) static int last_batch_seq_released;
__declspec(shared) static int poll_interval;
__declspec(shared) uint32_t mu_script_base_s8;

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

/** tx_slave_wait_feor_batch_seq
 * 5i * 90% / 15i+150 * 10% / poll if way ahead
 *
 * = 6i+15
 */
void
tx_slave_wait_for_batch_seq(struct tx_pkt_work *tx_pkt_work)
{

    for (;;) {
        __xread  struct batch_seq batch_seq;

        if ((tx_pkt_work->batch_seq - last_batch_seq_released) <
            MAX_BATCH_ITEMS_IN_PROCESSING)
            return;

        mem_atomic_read_s8(&batch_seq, base_s8, ofs, sizeof(batch_seq));
        last_batch_seq_released = batch_seq.last_released;

        if ((tx_pkt_work->batch_seq - last_batch_seq_released) <
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

    __asm {
        mem[packet_read_packet_status, pkt_status[0], \
            ctm_pkt_desc->pkt_num, 0, 1],             \
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
    script_size = sizeof(*script) / sizeof(uint64_t);
    __asm {
        mem[read, *script, mu_script_base_s8, <<8,\
            tx_pkt_work->script_offset, script_size], \
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
    int ctm_dma_len;
    ctm_dma_len = 0;
    if (tx_pkt_work.length>64)  ctm_len_in_64B=1;
    if (tx_pkt_work.length>128) ctm_len_in_64B=2;
    mu_base_lo = tx_pkt_work.mu_base_s8 << 8;
    bm = mu_base_hi;
    dm/ref = ctm_pkt_desc.pkt_addr >> 3;
    len = ctm_dma_len;
    __asm {
        mem[pe_dma_from_mu, blah, mu_base_lo, 64, 1], indirect_ref,
            sig_done[*sig];
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
        mem[packet_complete_unicast, --, pkt_num_s16, tx_pkt_length],
            indirect_ref;
    }
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
    batch_desc.override = ( (1<<0) | (1<<1) | (1<<3) | (1<<6) | (1<<7) );
    batch_desc.override |= (nbi<<12)<<16;
    batch_desc.override |= batch_desc.queue << 16;
    for (;;) {
        struct tx_pkt_work tx_pkt_work;
        struct ctm_pkt_desc ctm_pkt_desc;
        __xread struct script script;
        struct script_finish script_finish;
        SIGNAL dma_sig;
        SIGNAL script_sig;

        tx_slave_get_pkt_in_batch(&tx_pkt_work);
        if (tx_pkt_work.script_ofs == 0) {
            tx_slave_incr_batch_seq();
            continue;
        }
        tx_slave_wait_for_batch_seq(&tx_pkt_work);
        tx_slave_read_script_start(&tx_pkt_work, &script_sig);
        tx_slave_alloc_pkt(&ctm_pkt_desc);
        tx_slave_dma_pkt_start(&tx_pkt_work, &ctm_pkt_desc, &dma_sig);
        ctx_wait(&script_sig);
        tx_slave_script_start(&ctm_pkt_desc, &script, &script_finish);
        ctx_wait(&dma_sig);
        tx_slave_script_finish(&ctm_pkt_desc, &script_finish);
        tx_slave_wait_for_tx_time(&tx_pkt_work);
        tx_slave_pkt_tx(&tx_pkt_work, &ctm_pkt_desc);
        tx_slave_incr_batch_seq();
    }
}

/** batch_dist_add_pkt_to_batch
 */
__intrinsic void
batch_dist_add_pkt_to_batch(struct batch_work *batch_work,
                            struct flow_entry *flow_entry,
                            struct batch_desc *batch_desc,
                            __xwrite struct tx_pkt_work *tx_pkt_work_out,
                            int i,
                            SIGNAL *sig)
{
        tx_seq = batch_work->tx_seq;
        tx_pkt_work->tx_time_lo = batch_work->tx_time_lo + flow_entry->tx_time_lo;
        tx_pkt_work->tx_time_hi = batch_work->tx_time_hi + flow_entry->tx_time_hi; /* carry */
        tx_pkt_work->mu_base_s8 = flow_entry->mu_base_s8;
        tx_pkt_work->script_ofs = flow_entry->script_ofs;
        tx_pkt_work->batch_seq  = batch_work->batch_seq;
        tx_pkt_work->tx_seq     = tx_seq+i;
        tx_pkt_work_out[i] = tx_pkt_work;
        mem_workq_add_work_async(batch_desc->muq, (void *)tx_pkt_work_out,
                                 sizeof(tx_pkt_work), sig);
    }
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
        struct tx_pkt_work tx_pkt_work;
        struct ctm_pkt_desc ctm_pkt_desc;
        SIGNAL sig0, sig1, sig2, sig3, sig4, sig5, sig6, sig7;

        batch_dist_get_batch_work(&batch_work);
        batch_dist_get_flow_entries(&batch_work, &flow_entries[0]);
        batch_dist_add_pkt_to_batch(&batch_work, &flow_entries[0], &batch_desc[0], &sig0 );
        batch_dist_add_pkt_to_batch(&batch_work, &flow_entries[1], &batch_desc[1], &sig1 );
        batch_dist_add_pkt_to_batch(&batch_work, &flow_entries[2], &batch_desc[2], &sig2 );
        batch_dist_add_pkt_to_batch(&batch_work, &flow_entries[3], &batch_desc[3], &sig3 );
        batch_dist_add_pkt_to_batch(&batch_work, &flow_entries[4], &batch_desc[4], &sig4 );
        batch_dist_add_pkt_to_batch(&batch_work, &flow_entries[5], &batch_desc[5], &sig5 );
        batch_dist_add_pkt_to_batch(&batch_work, &flow_entries[6], &batch_desc[6], &sig6 );
        batch_dist_add_pkt_to_batch(&batch_work, &flow_entries[7], &batch_desc[7], &sig7 );
        wait_for_all(&sig0, &sig1, &sig2, &sig3, &sig4, &sig5, &sig6, &sig7);
    }
}

/** pktgen_master
 *
 * The master monitors the CLS and uses it to indicate that data is ready
 *
 * The shared structure contains:
 *
 * MU base address for script (do not know how to distribute yet)
 * MU address of batch base (do not know how to distribute yet)
 * Number of batches
 * Number of tx packets
 * Go indicator
 * Done indicator
 *
 * When started it distributes the work over the batch work queues
 *
 * The batches are credit-managed
 *
 * Probably need to do a number of batch_works at the same time to get
 * the rate up, since each batch work is 8 packets
 *
 * A burst of 4 at one time would be good.
 *
 */
void
pktgen_master(void)
{
    int tx_seq;
    tx_seq = 0;

    for (;;) {
        int batch_seq;
        int num_batches;
        int total_pkts;
        int40 base_time;
        batch_seq = 0;
        batch_work.base_time = now + delay;
        batch_work.work_ofs = 0;
        batch_work.num_valid_pkts = 8;
        while (num_batches>0) {
            if (too backed up) {
                me_sleep(poll_interval);
                read_backup();
                continue;
            }
            batch_work.tx_seq = tx_seq;
            batch_work.batch_seq = batch_seq;
            if (total_pkts<8) {
                batch_work.num_valid_pkts = total_pkts;
            }
            tx_master_add_batch_work(&batch_work);
            batch_work.work_ofs += 128;
            batch_seq += 1;
            tx_seq += 8;
            total_pkts -= 8;
            num_batches -= 1;
        }
        tx_seq -= ((-total_pkts)&7);
    }
}

