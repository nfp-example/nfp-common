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
#include "sync/stage.h"
#include "pktgen_lib.h"
#include <nfp/me.h>
#include <stdint.h>

/** Synchronization
 */
SYNC_STAGE_SET_PREINIT(PKTGEN_INIT_STAGES,PKTGEN_TX_CTXTS,PKTGEN_TX_MES,PKTGEN_ISLANDS);

/** main - Initialize, then run
 */
void main(void)
{
    int batch;
    batch = __MEID & 7;
    sync_state_set_stage_complete(PKTGEN_INIT_STAGE_CSR_INIT);
    //pktgen_tx_slave_init(batch); // gets queue configs
    sync_state_set_stage_complete(PKTGEN_INIT_STAGE_READY_TO_RUN);
    //pktgen_tx_slave();
    for (;;) {
        me_sleep(10000);
    }
}
