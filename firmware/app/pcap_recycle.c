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
#include "pcap.h"
#include "sync/stage.h"
#include "pcap_lib.h"
#include <stdint.h>

/** Static data
 */
#define NUM_MU_BUF 64
__asm {
    .alloc_mem   emu_buffer0    i24.mem global (PKT_CAP_MU_BUF_SIZE*NUM_MU_BUF) (1<<18);
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
        packet_capture_init_mu_buffer_recycler();
    } else {
        packet_capture_init_dma_to_host_slave();
    }
    sync_state_set_stage_complete(PCAP_INIT_STAGE_PREHOST_LOAD);
    if (ctx()==0) {
        uint32_t mu_base_s8;
        mu_base_s8 = (uint32_t)(__link_sym("emu_buffer0")>>8);
        packet_capture_fill_mu_buffer_list(mu_base_s8, NUM_MU_BUF);
    }
    sync_state_set_stage_complete(PCAP_INIT_STAGE_READY_TO_RUN);
    if (ctx()==0) {
        packet_capture_mu_buffer_recycler(poll_interval);
    } else {
        packet_capture_dma_to_host_slave();
    }
}
