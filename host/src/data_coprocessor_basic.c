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

/*t data_coproc_options */
/**
 */
struct data_coproc_options {
    int dev_num;
    int batch_size;
    int iterations;
    const char *firmware;
    const char *data_filename;
    const char *log_filename;
    int data_size;
};

/*a Global variables */
static const char *shm_filename="/tmp/nfp_dcb_shm.lock";
static int shm_key = 0x0d0c0b0a;
static const char *options = "b:d:f:i:hD:S:L:";
static struct option long_options[] = {
    {"help",       no_argument, 0,  'h' },
    {"batch-size", required_argument, 0, 'b' },
    {"device",     required_argument, 0, 'd' },
    {"firmware",   required_argument, 0, 'f' },
    {"iterations", required_argument, 0, 'i' },
    {"data-file",  required_argument, 0, 'D' },
    {"data-size",  required_argument, 0, 'S' },
    {"log-file",   required_argument, 0, 'L' },
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
data_coproc_initialize(struct data_coproc *data_coproc,
                       struct data_coproc_options *data_coproc_options,
                       size_t shm_size)
{
    data_coproc->nfp = nfp_init(data_coproc_options->dev_num, 1);
    if (!data_coproc->nfp) {
        fprintf(stderr, "Failed to open NFP\n");
        return 1;
    }

    if (nfp_fw_load(data_coproc->nfp, data_coproc_options->firmware) < 0) {
        fprintf(stderr, "Failed to load NFP firmware\n");
        return 2;
    }

    if (nfp_get_rtsym_cppid(data_coproc->nfp, "dcprc_init_csrs_included", NULL)<0) {
        fprintf(stderr, "Firmware is missing CSR initialization (symbol 'dcprc_init_csrs_included' is missing)\n");
        return 2;
    }

    //nfp_show_rtsyms(data_coproc->nfp);
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
                     uint64_t host_physical_address,
                     uint32_t operand_0,
                     uint32_t operand_1 )
{
    int wptr;
    int mask;
    struct dcprc_workq_entry *workq_entry;
    wptr = data_coproc->work_queues[queue].wptr;
    mask = data_coproc->work_queues[queue].max_entries-1;
    workq_entry = &(data_coproc->work_queues[queue].entries[wptr&mask]);

    workq_entry->work.host_physical_address = host_physical_address;
    workq_entry->work.operand_0 = operand_0;
    workq_entry->__raw[3] = 0x80000000 | operand_1;
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
    int iterations;

    rptr = data_coproc->work_queues[queue].rptr;
    mask = data_coproc->work_queues[queue].max_entries-1;
    workq_entry = &(data_coproc->work_queues[queue].entries[rptr & mask]);
    iterations = 0;
    while (workq_entry->result.not_valid) {
        int i;
        for (i=0; i<100; i++) {
            int j=SL_TIMER_CPU_CLOCKS;
            j = j;//fred += j;
            //usleep(10);
        }
        iterations++;
        if (iterations>0x80000) {
            fprintf(stderr,"%08x %08x %08x %08x\n",
                    workq_entry->__raw[0],
                    workq_entry->__raw[1],
                    workq_entry->__raw[2],
                    workq_entry->__raw[3] );
            fprintf(stderr,"Timeout waiting for data %d\n",rptr);
            exit(4);
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
static int
usage(int error)
{
    printf("Help goes here\n");
    if (error)
        return 4;
    return 0;
}

/*f run_test */
static void
run_test(struct data_coproc *data_coproc,
         struct data_coproc_options *data_coproc_options)
{
    t_sl_timer timer_do_work;
    t_sl_timer timer_add_work;
    uint64_t phys_addr;
    char *data_space;
    struct dcprc_workq_entry *log_buffer;
    FILE *log_file;

    int iterations;
    int batch_size;
    int data_size;

    int iter;

    iterations = data_coproc_options->iterations;
    batch_size = data_coproc_options->batch_size;
    data_size  = data_coproc_options->data_size;

    if (iterations<1) iterations=1;
    if (data_size<1024) data_size=1024;

    log_file = NULL;
    log_buffer = NULL;
    if (data_coproc_options->log_filename) {
        log_file=fopen(data_coproc_options->log_filename,"w");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file '%s'\n",data_coproc_options->log_filename);
            usage(1);
            return;
        }
        log_buffer = malloc(sizeof(struct dcprc_workq_entry)*iterations*batch_size);
        if (log_buffer==NULL) {
            fprintf(stderr, "Failed to malloc log buffer\n");
            usage(1);
            return;
        }
    }

    data_space = ((char *)data_coproc->shm_base) + 2*1024*1024;
    if (data_coproc_options->data_filename) {
        FILE *f;
        f = fopen(data_coproc_options->data_filename,"rb");
        if (!f) {
            fprintf(stderr,"Failed to open data-file '%s'\n", data_coproc_options->data_filename);
            usage(1);
            return;
        }
        data_size = fread(data_space, 1, 2*1024*1024, f);
        if (data_size==0) {
            usage(1);
            return;
        }
        fclose(f);
    } else {
        int i;
        for (i=0; i<data_size; i++) {
            data_space[i] = i;
        }
    }

    phys_addr = nfp_huge_physical_address(data_coproc->nfp, data_space, 0);
    SL_TIMER_INIT(timer_do_work);
    SL_TIMER_INIT(timer_add_work);
    for (iter=0; iter<iterations; iter++) {
        int i;
        SL_TIMER_ENTRY(timer_add_work);
        for (i=0; i<batch_size; i++) {
            data_coproc_add_work(data_coproc, 0, phys_addr, data_size, i );
        }
        SL_TIMER_EXIT(timer_add_work);
        SL_TIMER_ENTRY(timer_do_work);
        data_coproc_commit_work(data_coproc, 0);
        for (i=0; i<batch_size; i++) {
            struct dcprc_workq_entry *dcprc_workq_entry;

            dcprc_workq_entry = data_coproc_get_results(data_coproc, 0);
            if (log_buffer) {
                memcpy(log_buffer+i+iter*batch_size, dcprc_workq_entry, sizeof(*dcprc_workq_entry));
            }
        }
        SL_TIMER_EXIT(timer_do_work);
    }
    printf("Time adding work per iteration %fus\n",SL_TIMER_VALUE_US(timer_add_work)/iterations);
    printf("Time adding work per work item %fus\n",SL_TIMER_VALUE_US(timer_add_work)/iterations/batch_size);
    printf("Time doing work (from commit to all work) per iteration %fus\n",SL_TIMER_VALUE_US(timer_do_work)/iterations);
    printf("Time doing work (from commit to all work) per work item %fus\n",SL_TIMER_VALUE_US(timer_do_work)/iterations/batch_size);

    if (log_file) {
        int i, n;
        n = 0;
        for (iter=0; iter<iterations; iter++) {
            for (i=0; i<batch_size; i++) {
                fprintf(log_file, "%4d:%4d:%08x, %08x, %08x, %08x\n",
                        iter, i, 
                        log_buffer[n].__raw[0],
                        log_buffer[n].__raw[1],
                        log_buffer[n].__raw[2],
                        log_buffer[n].__raw[3] );
                n++;
            }
        }
        fclose(log_file);
        log_file = NULL;
    }
}

/*f read_options */
/**
 **/
static int
read_options(int argc, char **argv, struct data_coproc_options *data_coproc_options)
{
    data_coproc_options->dev_num = 0;
    data_coproc_options->batch_size=100;
    data_coproc_options->iterations=10000;
    data_coproc_options->firmware="firmware/nffw/data_coproc_null_one.nffw";
    data_coproc_options->data_filename=NULL;
    data_coproc_options->log_filename=NULL;
    data_coproc_options->data_size=0;

    for (;;) {
        int option_index = 0;
        int c = getopt_long(argc, argv, options, long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'd': {
            if (sscanf(optarg,"%d",&data_coproc_options->dev_num)!=1)
                return usage(1);
            break;
        }
        case 'b': {
            if (sscanf(optarg,"%d",&data_coproc_options->batch_size)!=1)
                return usage(1);
            break;
        }
        case 'i': {
            if (sscanf(optarg,"%d",&data_coproc_options->iterations)!=1)
                return usage(1);
            break;
        }
        case 'f': {
            data_coproc_options->firmware = optarg;
            break;
        }
        case 'S': {
            if (sscanf(optarg,"%d",&data_coproc_options->data_size)!=1)
                return usage(1);
            break;
        }
        case 'D': {
            data_coproc_options->data_filename = optarg;
            break;
        }
        case 'L': {
            data_coproc_options->log_filename = optarg;
            break;
        }
        case 'h': {
            return usage(0);
        }
        }
    }

    printf("data_coproc_options->dev_num %d\n",data_coproc_options->dev_num );
    printf("data_coproc_options->batch_size %d\n",data_coproc_options->batch_size);
    printf("data_coproc_options->iterations %d\n",data_coproc_options->iterations);
    printf("data_coproc_options->firmware '%s'\n",data_coproc_options->firmware);
    printf("data_coproc_options->data_filename '%s'\n",data_coproc_options->data_filename);
    printf("data_coproc_options->log_filename '%s'\n",data_coproc_options->log_filename);
    printf("data_coproc_options->data_size %d\n",data_coproc_options->data_size);
    
    return 0;
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
    struct data_coproc_options data_coproc_options;
    size_t shm_size = 16 * 2 * 1024 * 104;

    if (read_options(argc, argv, &data_coproc_options)!=0)
        return 4;

    if (data_coproc_initialize(&data_coproc, &data_coproc_options, shm_size)!=0)
        return 4;

    if ((data_coproc_options.batch_size<1) ||
        (data_coproc_options.batch_size>data_coproc.work_queues[0].max_entries-1)) {
        fprintf(stderr, "Batch size %d out of range 1..%d\n",
                data_coproc_options.batch_size,
                data_coproc.work_queues[0].max_entries-1);
        return 4;
    }

    run_test(&data_coproc, &data_coproc_options);

    data_coproc_shutdown(&data_coproc);
    return 0;
}
