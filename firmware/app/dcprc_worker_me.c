/*a Copyright */
/**
 * Copyright (C) 2015-2016,  Gavin J Stark.  All rights reserved.
 *
 * @file        dcprc_worker_me.c
 * @brief       Data coprocessor worker
 *
 * Not sure yet
 *
 */

/*a Includes
 */
#include "sync/stage.h"
#include "data_coproc_lib.h"
#include "dcprc_worker.h"
#include <stdint.h>

/*a Synchronization
 */
SYNC_STAGE_SET_GLOBALS(DCPRC_INIT_STAGES);

/*a Code */
/*f main */
/**
 */
void main(void)
{
    SYNC_STAGE_SET_PREINIT();

    sync_state_set_stage_complete(DCPRC_INIT_STAGE_CSR_INIT);

    // Could do the following just for context 0 if it sets up
    // shared registers. But it might not.
    dcprc_worker_thread_init();

    sync_state_set_stage_complete(DCPRC_INIT_STAGE_READY_TO_RUN);
    dcprc_worker_thread();
}
