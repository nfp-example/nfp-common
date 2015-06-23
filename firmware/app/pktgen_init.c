/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        pktgen_init.c
 * @brief       Packet generator initialization, sets up network and CSRs
 *
 */

/** Includes
 */
#include "pcap.h"
#include "sync/stage.h"
#include "pktgen_lib.h"
#include <stdint.h>

/** Defines
 */
#ifndef STRINGIFY
#define __STRINGIFY(x) #x
#define STRINGIFY(x) __STRINGIFY(x)
#endif

/** CSR initialization
 */
/* DMA config */
#define __PCI "pcie:i4.PcieInternalTargets.DMAController"
#define __DCFG __PCI ".DMADescrConfig" STRINGIFY(PKTGEN_PCIE_DMA_CFG)
_init_csr( __DCFG ".CppTargetIDEven     0x7 const");
_init_csr( __DCFG ".Target64bitEven     0   const");
_init_csr( __DCFG ".NoSnoopEven         0   const");
_init_csr( __DCFG ".RelaxedOrderingEven 0   const");
_init_csr( __DCFG ".IdBasedOrderingEven 0   const");
_init_csr( __DCFG ".StartPaddingEven    0   const");
_init_csr( __DCFG ".EndPaddingEven      0   const");
_init_csr( __DCFG ".SignalOnlyEven      0   const");
#undef __PCI

/* Debug CLS ring
 */
_init_csr("cls:i4.Rings.RingBase0.Size 5")
_init_csr("cls:i4.Rings.RingBase0.Base 0x1e0")
_init_csr("cls:i4.Rings.RingBase0.Full 0")
_init_csr("cls:i4.Rings.RingBase0.NotEmpty 0")
_init_csr("cls:i4.Rings.RingPtrs0.HeadPointer 0")
_init_csr("cls:i4.Rings.RingPtrs0.TailPointer 0")

/** Synchronization
 */
SYNC_STAGE_SET_PREINIT(PKTGEN_INIT_STAGES,PKTGEN_INIT_CTXTS,PKTGEN_INIT_MES,PKTGEN_ISLANDS);

/** struct tmq_config
 */
struct tmq_config {
    int first_queue;
    int num_queues;
    int first_entry;
    int log2_entries_per_queue;
};

/** Constant TM Queue configurations
 */
const struct tmq_config tmq_config_1024_of_16 = {
    0, 1024, 0, 4
};
const struct tmq_config tmq_config_all_disabled = {
    0, 1024, 0, 0
};
const struct tmq_config tmq_config_q0_16k = {
    0, 1, 0, 14
};

/** nbi_write64_s8
 *
 * @param data     Transfer registers to write
 * @param base_s8  Base address in MU >> 8
 * @param ofs      Offset in bytes from MU base
 * @param size     Size in bytes to write (must be multiple of 8)
 *
 */
__intrinsic void
nbi_write64_s8(__xwrite void *data,
               uint32_t base_s8,
               uint32_t ofs,
               const size_t size)
{
    SIGNAL sig;
    uint32_t size_in_uint64;

    size_in_uint64 = size >> 3;
    __asm {
        nbi[write, *data, base_s8, <<8, ofs, \
            __ct_const_val(size_in_uint64)], ctx_swap[sig];
    }
}

/** network_base_init
 */
void network_base_init(void)
{
}

/** network_init_ctm
 *
 * Initialize CTM for network traffic (receive or transmit)
 *
 * @param island        Island number of CTM to initialize
 * @param mem_for_pkts  0 to use all CTM for packets, 1 for 1/2
 *
 * On later devices a value of mem_for_pkts of 2 and 5 are one or three quarters
 * and 3, 4, 6, 7 are one, seven, five and three eights respectively
 *
 * The CTM is assumed to have been correctly reset (i.e. its packet work queue is
 * already empty)
 *
 */
void network_init_ctm(int island, int mem_for_pkts)
{
    uint32_t xpb_base;
    xpb_base = ( (1 << 31) |
                 (island << 24) |
                 (7 << 16) );
    xpb_write(xpb_base, 0, mem_for_pkts);
}

/** network_init_npc
 *
 * @param island Island to configure
 *
 */
void network_init_npc(int island)
{
    uint32_t xpb_base_char;
    uint32_t xpb_base_pico;

    xpb_base_pico = ( (1 << 31) |
                      (island << 24) |
                      (0x28<<16) );
    xpb_base_char = ( (1 << 31) |
                      (island << 24) |
                      (0x29<<16) );

    xpb_write(xpb_base_char, 0, 0x32ff0000 ); // 50 packets in classification, 255 buffers max
    xpb_write(xpb_base_pico, 0, 0x00050007 ); // Shared memories and 48 picoengines
    xpb_write(xpb_base_pico, 4, 0x00000040 ); // 16-bit picoengine sequencer value
    xpb_write(xpb_base_pico, 8, 0x3ffffff1 ); // Enable picoengines and memories and start
}

/** init_dma_buffer_list - initialize a buffer list in an NBI DMA
 *
 * @param nbi_island    The NBI island number (8 or 9) to initialize
 * @param buffer_list   The buffer list to be configured
 * @param num_buffers   Number of buffers to provide the list initially
 * @param base          Base address, including MU island and addressing, of
 *                      first buffer to add
 * @param stride        Separation in bytes of the buffers in MU address space
 *
 * 
 */
void init_dma_buffer_list(int island, int buffer_list, int num_buffers, uint64_t base, uint32_t stride)
{
    int base_s8;
    int ofs;
    int i;
    uint32_t data;
    __xwrite uint32_t data_out[2];

    base_s8 = ( ((island & 3) << 30) |
                (0<<12) ); // Magic to address through CPP the DMA memories;
    data = base >> 11;
    data_out[1] = 0;
    for (i = 0; i < num_buffers; i++) {
        ofs = 0x0000 + (i << 3);
        data_out[0] = data;
        nbi_write64_s8(data_out, base_s8, ofs, sizeof(data_out));
        data += (stride >> 11);
    }
    ofs = 0x8000 + (buffer_list << 3); // base of 0x68000 for TMHeadTailSram
    data_out[0] = num_buffers; // MUST BE LESS THAN 512 SINCE WE ARE SETTING SIZE TO 512
    nbi_write64_s8(data_out, base_s8, ofs, sizeof(data_out));
}

/** network_init_dma
 * split_length 3 -> 2kB split
 * ctm_offset 1 -> 32B
 */
void network_init_dma(int island, int ctm_offset, int split_length)
{
    int i;
    uint32_t xpb_base_nbi_dma;

    xpb_base_nbi_dma = ( (1 << 31) |
                         (island << 24) |
                         (0x10<<16) );
    xpb_write(xpb_base_nbi_dma, 0, ( (((island & 3) + 1) << 7) |
                                     (1 << 6)) ); // Set island number and enable CTM polling
    xpb_write(xpb_base_nbi_dma, 0x20, ( (split_length << 5) |
                                        (ctm_offset << 12) |
                                        (0 << 13)) ); /* No drop */

    /* Disable all BPEs */
    for (i = 0; i < 32; i++) {
        xpb_write(xpb_base_nbi_dma, 0x40 | (i << 2), 0 );
    }

    xpb_write(xpb_base_nbi_dma, 0x40, ((4 << 21) | /* CTM 4 */
                                       (64 << 10) | /* 64 packet credits */
                                       (64 << 0)) ); /* 64 2kB buffer credits */

    /* Mark BPE0 is end of chain
     */
    xpb_write(xpb_base_nbi_dma, 0x18, (1<<0) );

    init_dma_buffer_list(island, 0, 128, ((2LL<<38)|(28LL<<32)|0LL), 2048 );
}

/** network_init_tm_queues
 *
 * @param island      Island in which to configure queues
 * @param tmq_config  TMQ configuration
 * 
 * Write the head/tail SRAM and queue config for a range of queues
 *
 */
__intrinsic void network_init_tm_queues(int island,
                                        const struct tmq_config *tmq_config)
{
    uint32_t nbi_base_s8;
    uint32_t xpb_base_nbi;
    SIGNAL sig;
    int queue, entry, log2_size, enable;
    int sram_offset, xpb_offset;
    int i;

    nbi_base_s8 = ( ((island & 3) << 30) |
                    (2<<12) |
                    0x680 ); /* Head/tail SRAM base */
    xpb_base_nbi = ( (1 <<  31) |
                     (island << 24) |
                     (0x15 << 16) |
                     0x1000 );
    
    queue = tmq_config->first_queue;
    entry = tmq_config->first_entry;
    log2_size = tmq_config->log2_entries_per_queue;
    enable = 1;
    if (log2_size == 0) { enable = 0; }
    sram_offset = queue << 3;
    xpb_offset = queue << 2;

    for (i=0; i<tmq_config->num_queues; i++) {
        uint64_t head_tail;
        __xwrite uint64_t head_tail_out;

        xpb_write(xpb_base_nbi, xpb_offset, (log2_size << 6) | enable );

        head_tail = entry << 4;
        head_tail = (head_tail << 14) | head_tail;

        head_tail_out = head_tail << 32; /* As the compiler is BE, HW is LWBE */
        nbi_write64_s8(&head_tail_out, nbi_base_s8, sram_offset, sizeof(head_tail_out));
        sram_offset += 8;
        xpb_offset  += 4;
        entry += 1 << log2_size;
    }
}

/** network_init_tm - Initialize TM CSRS (could be init_csrs perhaps now)
 *
 * @param island Island to configure
 * 
 */
static void network_init_tm(int island)
{
    uint32_t xpb_base_nbi_tm = ( (1 << 31) |
                                 (island << 24) |
                                 (0x14 << 16) );

    xpb_write(xpb_base_nbi_tm, 0, 0x1d40 ); /* MiniPacketFCEnable, NumSequencers 0, SchedulerEnable,
                                            Sequencer0Enable,
                                            DescQueuesEnable, LevelCheckEnable */
    xpb_write(xpb_base_nbi_tm, 0x0300, 0x1200014); /* CreditLimit 0x14, FPCreditLimit 0x200, MiniPacketFCMode 1*/
    xpb_write(xpb_base_nbi_tm, 8, 0xf); /* BLQEventStatusEnable=0xf */
}

/** main - Initialize, then run
 */
void main(void)
{
    int i;

    if (ctx()!=0) {
        __asm { ctx_arb[kill] }
    }

    network_base_init();

    /* Rx */
    //network_init_npc(8);
    //network_init_dma(8);

    /* Tx */
    network_init_tm(8);
    network_init_tm_queues(8, &tmq_config_all_disabled);
    network_init_tm_queues(8, &tmq_config_q0_16k);
    for (i=32; i<32+PKTGEN_TX_ISLANDS; i++) {
        network_init_ctm(i, 1); /* Half of CTM for pkts */
    }

    sync_state_set_stage_complete(PKTGEN_INIT_STAGE_READY_TO_RUN);
    __asm { ctx_arb[kill] }
}
