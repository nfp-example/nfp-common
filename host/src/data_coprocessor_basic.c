/*a Copyright */
/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
#include "nfp_support.h"
#include "timer.h"

/*a Types */
/*t data_coproc */
/**
 **/
struct data_coproc {
    struct nfp *nfp;
    int cls_host_wptrs;
    void *shm_base;
    uint64_t phys_addr[1];
};

/*a Global variables */
static nffw_filename="banana.nffw";
static shm_filename="fred";
static shm_key = 0;

/*a Functions
 */
/*f data_coproc_initialize */
/**
 **/
static int data_coproc_initialize(struct data_coproc *data_coproc, int dev_num, size_t shm_size)
{
    data_coproc->nfp = nfp_init(dev_num);
    if (!data_coproc->nfp) {
        fprintf(stderr, "Failed to open NFP\n");
        return 1;
    }

    if (nfp_fw_load(data_coproc->nfp, nffw_filename) < 0) {
        fprintf(stderr, "Failed to load NFP firmware\n");
        return 1;
    }

    if (nfp_get_rtsym_cppid(data_coproc->nfp,
                            "i4.cls_host_wptrs",
                            &data_coproc->cls_host_wptrs) < 0) {
        fprintf(stderr, "Failed to find necessary symbols\n");
        return 1;
    }

    if (nfp_shm_alloc(data_coproc->nfp,
                      shm_filename, shm_key,
                      shm_size, 1)==0) {
        return 1;
    }

    data_coproc->shm_base = nfp_shm_data(data_coproc->nfp);
    memset(data_coproc->shm_base, 0, shm_size);
    data_coproc->phys_addr[0] = nfp_huge_physical_address(data_coproc->nfp,
                                                         data_coproc->shm_base,
                                                         0);
    if (data_coproc->phys_addr[0] == 0) {
        fprintf(stderr, "Failed to find physical page mapping\n");
        return 1;
    }

    if (nfp_fw_start(data_coproc->nfp)<0) {
        fprintf(stderr,"Failed to start NFP firmware\n");
        return 4;
    }
}

/*f data_coproc_shutdown */
/**
 **/
static void
data_coproc_shutdown(struct data_coproc *data_coproc)
{
    nfp_shutdown(data_coproc->nfp);
}

/*f main */
/**
    For this we load the firmware and give it packets.
 */
extern int
main(int argc, char **argv)
{
    struct data_coproc data_coproc;
    int dev_num = 0;
    size_t shm_size = PCIE_HUGEPAGE_SIZE * MAX_PAGES;

    data_coproc_initialize(&data_coproc, dev_num, shm_size);

    for (;;) {
    }

    data_coproc_shutdown(&data_coproc);
    return 0;
}
