/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
 *
 * @file        pktgen_host.c
 * @brief       Packet generator host interaction
 *
 * Instantiate a host interaction master and seven distributors
 *
 */

/** Includes
 */
#include "sync/stage.h"
#include "pktgen_lib.h"
#include <stdint.h>

/** Synchronization
 */
SYNC_STAGE_SET_PREINIT(PKTGEN_INIT_STAGES,PKTGEN_HOST_CTXTS,PKTGEN_HOST_MES,PKTGEN_ISLANDS);

/** Allocate some buffer space for the host to use for schedule/packets
 */
__asm {
    .alloc_mem   pktgen_emu_buffer0    i24.mem global (2<<20) 4096;
};

/** main - Initialize, then run
 */
void main(void)
{
    sync_state_set_stage_complete(PKTGEN_INIT_STAGE_PREHOST_LOAD);
    if (ctx()==0) {
        pktgen_master_init();
    } else {
        pktgen_batch_distributor_init();
    }
    sync_state_set_stage_complete(PKTGEN_INIT_STAGE_READY_TO_RUN);
    if (ctx()==0) {
        pktgen_master();
    } else {
        pktgen_batch_distributor();
    }
}
