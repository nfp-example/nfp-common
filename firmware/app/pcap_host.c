/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        pcap_host.c
 * @brief       Packet capture host interaction
 *
 * Instantiate a single DMA master and seven DMA slaves.
 *
 */

/** Includes
 */
#include "sync/stage.h"
#include "pcap_lib.h"
#include <stdint.h>

/** Synchronization
 */
SYNC_STAGE_SET_PREINIT(PCAP_INIT_STAGES,PCAP_HOST_CTXTS,PCAP_HOST_MES,PCAP_ISLANDS);

/** main - Initialize, then run
 */
void main(void)
{
    int poll_interval;
    poll_interval = 1000;

    sync_state_set_stage_complete(PCAP_INIT_STAGE_CSR_INIT);

    if (ctx()==0) {
        packet_capture_init_dma_to_host_master();
    } else {
        packet_capture_init_dma_to_host_slave();
    }

    sync_state_set_stage_complete(PCAP_INIT_STAGE_READY_TO_RUN);
    if (ctx()==0) {
        packet_capture_dma_to_host_master(poll_interval);
    } else {
        packet_capture_dma_to_host_slave();
    }
}
