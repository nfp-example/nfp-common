/*a Copyright */
/**
 * Copyright (C) 2015-2016,  Gavin J Stark.  All rights reserved.
 *
 * @file        data_coproc_host.c
 * @brief       Data coprocessor host interaction
 *
 * Not sure yet
 *
 */

/*a Includes
 */
#include "sync/stage.h"
#include <stdint.h>
#define DCPRC_INIT_CSRS
#include "data_coproc_lib.h"

/*a Synchronization
 */
SYNC_STAGE_SET_GLOBALS(DCPRC_INIT_STAGES);

/*a Code */
/*f main */
/**
 */
void main(void)
{
    int poll_interval;
    poll_interval = 1000;

    SYNC_STAGE_SET_PREINIT();

    sync_state_set_stage_complete(DCPRC_INIT_STAGE_CSR_INIT);

    if (ctx()==0) {
        data_coproc_init_workq_manager(poll_interval);
    } else {
    }

    sync_state_set_stage_complete(DCPRC_INIT_STAGE_READY_TO_RUN);
    if (ctx()==0) {
        data_coproc_workq_manager(32);
    } else {
        data_coproc_work_gatherer();
    }
}
