/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        pcap_recycle.c
 * @brief       Packet capture buffer recycler (gets host buffers)
 *
 * Instantiate the MU buffer recycler - exactly one is needed in the
 * system - and seven threads of packet slave DMA.
 *
 */

/** Includes
 */
#include "sync/stage.h"
#include "pcap_lib.h"
#include <stdint.h>
#include "network/init.h"

/** Static data
 */
#define NUM_MU_BUF 64
__asm {
    .alloc_mem   pcap_emu_buffer0    i24.mem global (PKT_CAP_MU_BUF_SIZE*NUM_MU_BUF) (1<<18);
};

/** Synchronization
 */
SYNC_STAGE_SET_PREINIT(PCAP_INIT_STAGES,PCAP_HOST_CTXTS,PCAP_HOST_MES,PCAP_ISLANDS);

/** main - Initialize, then run
 */
void main(void)
{
    int poll_interval;
    poll_interval = 1000;

    if (ctx()==0) {
        int nbi_island;
        int ctm_island;
        int i;

        for (nbi_island=8; nbi_island<9; nbi_island++) {
            network_npc_control(nbi_island,0); // Disable packets
        }
        for (ctm_island=32; ctm_island<32+PCAP_RX_ISLANDS; ctm_island++) {
            //network_ctm_cleanup(ctm_island, 2000);
        }
        //init_tm(nbi_island);
        network_npc_init(8);
        network_dma_init(8);
        network_dma_init_buffer_list(8, 0 /* buffer list 0 */,
                                         128 /* 128 buffers */,
                                         ((2LL<<38)|(28LL<<32)|0LL), /* MU base address */
                                         2048 /* MU buffer stride */
            );
        i = network_dma_init_bp(8,
                                0 /* buffer pool */,
                                0 /* BPE start */,
                                1,/* 64B CTM offset */
                                3 /* 2kB split length */
            );
        for (ctm_island=32; ctm_island<32+PCAP_RX_ISLANDS; ctm_island++) {
            i = network_dma_init_bpe(8, 0, i, ctm_island, 64 /*pkt credit*/, 64 /*buffer credit */ );
        }
        network_dma_init_bp_complete(8, 0, i);
        for (ctm_island=32; ctm_island<32+PCAP_RX_ISLANDS; ctm_island++) {
            network_ctm_init(ctm_island, 0);
        }
    }
    sync_state_set_stage_complete(PCAP_INIT_STAGE_CSR_INIT);

    if (ctx()==0) {
        packet_capture_init_mu_buffer_recycler();
    } else {
        packet_capture_init_dma_to_host_slave();
    }

    sync_state_set_stage_complete(PCAP_INIT_STAGE_PREHOST_LOAD);

    if (ctx()==0) {
        uint32_t mu_base_s8;
        mu_base_s8 = (uint32_t)(__link_sym("pcap_emu_buffer0")>>8);
        packet_capture_fill_mu_buffer_list(mu_base_s8, NUM_MU_BUF);
    }
    sync_state_set_stage_complete(PCAP_INIT_STAGE_READY_TO_RUN);
    if (ctx()==0) {
        network_npc_control(8,1); // Enable packets
        packet_capture_mu_buffer_recycler(poll_interval);
    } else {
        packet_capture_dma_to_host_slave();
    }
}
