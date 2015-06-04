/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        pcap_rx.c
 * @brief       Packet capture receiver
 *
 * Instantiate eight packet receive threads, supporting up to 8Mpps
 * for the packet capture into MU buffers.
 *
 */

/** Includes
 */
#include "pcap.h"
#include "sync/stage.h"
#include "pcap_lib.h"
#include <stdint.h>

/** Synchronization
 */
SYNC_STAGE_SET_PREINIT(PCAP_INIT_STAGES,PCAP_RX_CTXTS,PCAP_RX_MES,PCAP_ISLANDS);

/** main - Initialize, then run
 */
void main(void)
{
    int poll_interval;
    poll_interval = 1000;

    packet_capture_init_pkt_rx_dma();
    sync_state_set_stage_complete(PCAP_INIT_STAGE_READY_TO_RUN);
    packet_capture_pkt_rx_dma(poll_interval);
}
