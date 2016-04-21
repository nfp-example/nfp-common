/*a Copyright */
/**
 Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 * @file        data_coproc_lib.c
 * @brief       A simple data coprocessor
 *
 *  @dot
 *  digraph example {
 *      node [shape=record, fontname=Helvetica, fontsize=10];
 *      b [ label="work queue manager" URL="\ref B"];
 *      c [ label="class C" URL="\ref C"];
 *      b -> c [ arrowhead="open", style="dashed" ];
 *  }
 *  @enddot
 */

/*a Includes
 */
#include <stdint.h>
#include <nfp/me.h>
#include <nfp/mem.h>
#include <nfp/cls.h>
#include <nfp/pcie.h>
#include <nfp/types.h>
#include <nfp.h>
#include <nfp_override.h>
#include "firmware/data_coproc.h"
#include "data_coproc_lib.h"

/*a Defines
 */
#define U32_LINK_SYM(symname,shift) \
    ((uint32_t)(((uint64_t)__link_sym(#symname))>>shift))

/* DMA_MAX_BURST is the largest DMA burst to do
   All DMAs should be 64B aligned on host and MU, so 1kB is fine
 */
#define DMA_MAX_BURST 1024

/*a Static data used globally */
/*a Types */
/*f do_work */
static void
do_work(int queue_number)
{
//    get_next_mu_work_address(and add up to 4);
//    start_dma_of_up_to_four_desc_from_workq();
//    add_up_to_four_to_rptr();
//    add_work_to_internal_workqueue(); // work is queue number, MU work address (16B there)
    // use credits for internal work queue
    // have a cache of credits in ME
    // Out of kindness, atomically add number to 'write ptr' in MU when adding work
    // 'completed_ptr' incremented when workers complete all the processing
    // cache that completed_ptr in a caching thread
    // caching thread can also manage which queues are active
    // THAT should be the workq manager...
    // work gatherer should DMA work from host to MU...
}

/*f data_coproc_workq_manager */
/**
 * @callgraph
 *  @dot
 *  digraph workq_manager {
 *      node [shape=record, fontname=Helvetica, fontsize=10];
 *      a [ label="work_in_hand?" URL="\ref B"];
 *      b [ label="read_write_pointer" URL="\ref B"];
 *      c [ label="determine if work ready" URL="\ref C"];
 *      d [ label="Work ready?" URL="\ref C"];
 *      e [ label="SLEEP" URL="\ref C"];
 *      f [ label="launch_work" URL="\ref C"];
 *      a -> b, f;
 *      b -> c;
 *      c -> d;
 *      d -> e, f;
 *      e -> a;
 *      f -> a;
 *  }
 *  @enddot
 */
__intrinsic void
data_coproc_workq_manager(void)
{
    /*    for (;;) {
        if (!has_work) {
            read_wptr();
            has_work = (wptr!=rptr);
            if (!has_work) {
                sleep;
                continue;
            }
        }
        if (wptr<0) {
            disable_queue();
            sleep_forever();
        }
        do_work();
        }*/
}

/** data_coproc_init_workq_manager
 */
__intrinsic void
data_coproc_init_workq_manager(void)
{
//    read_workq_desc_into_lmem();
//    init_cls_workq();
}

