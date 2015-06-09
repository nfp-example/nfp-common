/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        pktgen_tx_slave.c
 * @brief       Packet generator transmitter slave
 *
 * Instantiate eight transmitter slaves, all working on the same batch
 *
 */

/** Includes
 */
#include "pcap.h"
#include "sync/stage.h"
#include "pktgen_lib.h"
#include <stdint.h>

/** Synchronization
 */
SYNC_STAGE_SET_PREINIT(PKTGEN_INIT_STAGES,PKTGEN_TX_CTXTS,PKTGEN_TX_MES,PKTGEN_ISLANDS);

/** main - Initialize, then run
 */
void main(void)
{
    pktgen_tx_slave_init();
    sync_state_set_stage_complete(PKTGEN_INIT_STAGE_READY_TO_RUN);
    pktgen_tx_slave();
}
