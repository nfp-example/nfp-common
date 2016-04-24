/*a Copyright */
/** Copyright (C) 2015-2016,  Gavin J Stark.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file  data_coprocessor_basic.c
 * @brief Simple basic data coprocessor
 *
 * This is a simple data coprocessor example, using just the
 * nfp_support subsystem to interact with an NFP card that provides
 * some basic data accelerations
 *
 */

/*a Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <string.h> 
#include <inttypes.h>
#include <getopt.h>
#include "nfp_support.h"
#include "timer.h"
#include "firmware/data_coproc.h"

/*a Types */
/*t data_coproc_work_queue */
/**
 **/
struct data_coproc_work_queue {
    struct dcprc_workq_entry *entries;
    uint64_t phys_addr;
    int      max_entries;
    int      wptr;
    int      rptr;
};

/*t data_coproc */
/**
 **/
struct data_coproc {
    struct nfp *nfp;
    struct nfp_cppid cls_workq;
    void *shm_base;
    uint64_t phys_addr[1];
    struct data_coproc_work_queue work_queues[1];
};

/*a Global variables */
static const char *nffw_filename="firmware/nffw/data_coproc.nffw";
static const char *shm_filename="/tmp/nfp_dcb_shm.lock";
static int shm_key = 0x0d0c0b0a;
static const char *options = "hf:";
static struct option long_options[] = {
    {"help",     no_argument, 0,  'h' },
    {"firmware", required_argument, 0,  'f' },
    {0,         0,                 0,  0 }
    };

/*a Functions
 */
/*f data_coproc_initialize */
/**
 * @brief Initialize data coprocessor - nfp, shm, firmware
 *
 * @param data_coproc Data coprocessor structure to fill out
 *
 * @param dev_num Device number of NFP to use
 *
 * @param shm_size Amount of shared memory to allocate
 *
 * @returns Zero on success, non-zero on error (with an error message
 * printed)
 *
 * Initialize the NFP and load the firmware; get everything that is
 * needed to interact (run-time symbols); allocate shared memory (to
 * get memory that can be shared with the NFP)
 *
 **/
static int
data_coproc_initialize(struct data_coproc *data_coproc, int dev_num, size_t shm_size)
{
    data_coproc->nfp = nfp_init(dev_num, 1);
    if (!data_coproc->nfp) {
        fprintf(stderr, "Failed to open NFP\n");
        return 1;
    }

    if (nfp_fw_load(data_coproc->nfp, nffw_filename) < 0) {
        fprintf(stderr, "Failed to load NFP firmware\n");
        return 2;
    }

    if (nfp_get_rtsym_cppid(data_coproc->nfp, "dcprc_init_csrs_included", NULL)<0) {
        fprintf(stderr, "Firmware is missing CSR initialization (symbol 'dcprc_init_csrs_included' is missing)\n");
        return 2;
    }

    nfp_show_rtsyms(data_coproc->nfp);
    if (nfp_sync_resolve(data_coproc->nfp)<0) {
        fprintf(stderr, "Failed to resolve firmware synchronization configuration - firmware would not start correctly\n");
        return 3;
    }

    if (nfp_get_rtsym_cppid(data_coproc->nfp,
                            "cls_workq",
                            &data_coproc->cls_workq) < 0) {
        fprintf(stderr, "Failed to find necessary symbols\n");
        return 3;
    }

    if (nfp_shm_alloc(data_coproc->nfp,
                      shm_filename, shm_key,
                      shm_size, 1)==0) {
        return 4;
    }

    data_coproc->shm_base = nfp_shm_data(data_coproc->nfp);
    memset(data_coproc->shm_base, 0, shm_size);
    data_coproc->phys_addr[0] = nfp_huge_physical_address(data_coproc->nfp,
                                                         data_coproc->shm_base,
                                                         0);
    if (data_coproc->phys_addr[0] == 0) {
        fprintf(stderr, "Failed to find physical page mapping\n");
        return 5;
    }

    data_coproc->work_queues[0].entries   = (struct dcprc_workq_entry *)data_coproc->shm_base;
    data_coproc->work_queues[0].phys_addr = nfp_huge_physical_address(data_coproc->nfp,
                                                                      data_coproc->work_queues[0].entries,
                                                                      0);
    data_coproc->work_queues[0].max_entries = 256;
    data_coproc->work_queues[0].wptr = 0;
    data_coproc->work_queues[0].rptr = 0;

    struct dcprc_workq_buffer_desc workq;
    workq.host_physical_address = data_coproc->work_queues[0].phys_addr;
    workq.max_entries           = data_coproc->work_queues[0].max_entries;
    workq.wptr                  = data_coproc->work_queues[0].wptr;

    if (nfp_write(data_coproc->nfp, &data_coproc->cls_workq, offsetof(struct dcprc_cls_workq, workqs[0]),
                  &workq, sizeof(workq))<0) {
        fprintf(stderr,"Failed to configure firmware with work queues\n");
        return 6;
    }

    if (nfp_fw_start(data_coproc->nfp)<0) {
        fprintf(stderr,"Failed to start NFP firmware\n");
        return 7;
    }
    return 0;
}

/*f data_coproc_shutdown */
/**
 * @brief data_coproc_shutdown
 *
 * @param data_coproc Data coprocessor structure already initialized
 *
 * Shuts down the NFP
 *
 **/
static void
data_coproc_shutdown(struct data_coproc *data_coproc)
{
    struct dcprc_workq_buffer_desc workq;
    workq.host_physical_address = 0;
    workq.max_entries = 0;
    workq.wptr        = -1;
    nfp_write(data_coproc->nfp, &data_coproc->cls_workq, offsetof(struct dcprc_cls_workq, workqs[0]),
              &workq, sizeof(workq));
    // wait
    // check coprocessor has shut down
    nfp_shutdown(data_coproc->nfp);
}

/*f data_coproc_add_work */
static void
data_coproc_add_work(struct data_coproc *data_coproc,
                     int queue,
                     uint64_t host_physical_address )
{
    int wptr;
    int mask;
    struct dcprc_workq_entry *workq_entry;
    wptr = data_coproc->work_queues[queue].wptr;
    mask = data_coproc->work_queues[queue].max_entries-1;
    workq_entry = &(data_coproc->work_queues[queue].entries[wptr&mask]);

    workq_entry->work.host_physical_address = host_physical_address;
    workq_entry->work.operand_0 = 0x12345678;
    workq_entry->__raw[3] = 0xdeadbeef;
    data_coproc->work_queues[queue].wptr = (wptr+1) & DCPRC_WORKQ_PTR_CLEAR_MASK;
}

/*f data_coproc_commit_work */
static void
data_coproc_commit_work(struct data_coproc *data_coproc,
                        int queue )
{
    int wptr;
    wptr = data_coproc->work_queues[queue].wptr;
    nfp_write(data_coproc->nfp,
              &data_coproc->cls_workq,
              offsetof(struct dcprc_cls_workq, workqs[queue].wptr),
              &wptr, sizeof(wptr));
}

/*f data_coproc_get_results */
volatile int fred;
static struct dcprc_workq_entry *
data_coproc_get_results(struct data_coproc *data_coproc,
                        int queue)
{
    int rptr;
    int mask;
    struct dcprc_workq_entry *workq_entry;

    rptr = data_coproc->work_queues[queue].rptr;
    mask = data_coproc->work_queues[queue].max_entries-1;
    workq_entry = &(data_coproc->work_queues[queue].entries[rptr & mask]);
    while (workq_entry->result.not_valid) {
        int i;
        for (i=0; i<100; i++) {
            int j=SL_TIMER_CPU_CLOCKS;
            j = j;//fred += j;
            //usleep(10);
        }
    };
    rptr += 1;
    data_coproc->work_queues[queue].rptr = rptr;
    return workq_entry;
}

/*f usage */
/**
 * Display help
 */
static void
usage(void)
{
    printf("Help goes here\n");
}

/*f main */
/**
 * Initialize the system, run data, and stop
 *
 *
 */
extern int
main(int argc, char **argv)
{
    struct data_coproc data_coproc;
    int dev_num = 0;
    size_t shm_size = 16 * 2 * 1024 * 104;

    for (;;) {
        int option_index = 0;
        int c = getopt_long(argc, argv, options, long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'f': {
            printf("option d with value '%s'\n", optarg);
            break;
        }
        case 'h': {
            usage();
            return 0;
        }
        }
    }

    if (data_coproc_initialize(&data_coproc, dev_num, shm_size)!=0)
        return 4;

    if (1) {
        t_sl_timer timer;
        int i;
        for (i=0; i<100; i++) {
            data_coproc_add_work(&data_coproc, 0, 0x123456789abcd00ll+i );
        }
        SL_TIMER_INIT(timer);
        SL_TIMER_ENTRY(timer);
        data_coproc_commit_work(&data_coproc, 0);
        for (i=0; i<100; i++) {
            struct dcprc_workq_entry *dcprc_workq_entry;

            dcprc_workq_entry = data_coproc_get_results(&data_coproc, 0);
            dcprc_workq_entry =dcprc_workq_entry;
/*            fprintf(stderr,"%8x\n",dcprc_workq_entry->__raw[0]);
            fprintf(stderr,"%8x\n",dcprc_workq_entry->__raw[1]);
            fprintf(stderr,"%8x\n",dcprc_workq_entry->__raw[2]);
            fprintf(stderr,"%8x\n",dcprc_workq_entry->__raw[3]);*/
        }
        SL_TIMER_EXIT(timer);
        printf("From commit to all work done took %fus\n",SL_TIMER_VALUE_US(timer));
    }
    
    data_coproc_shutdown(&data_coproc);
    return 0;
}
