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
#include "data_coproc_lib.h"
#include <stdint.h>

/*a Synchronization
 */
SYNC_STAGE_SET_PREINIT(DCPRC_INIT_STAGES,8,DCPRC_MES_PCIE0,DCPRC_ISLANDS);
SYNC_STAGE_SET_PREINIT_ME(8);

/*a Code */
/*f main */
/**
 */
void main(void)
{
    int poll_interval;
    poll_interval = 1000;

    sync_state_set_stage_complete(DCPRC_INIT_STAGE_CSR_INIT);

    if (ctx()==0) {
        data_coproc_init_workq_manager();
    } else {
        //packet_capture_init_dma_to_host_slave();
    }

    sync_state_set_stage_complete(DCPRC_INIT_STAGE_READY_TO_RUN);
    if (ctx()==0) {
        data_coproc_workq_manager();
    } else {
        //packet_capture_dma_to_host_slave();
    }
}
