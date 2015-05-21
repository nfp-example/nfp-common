/* Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 * @file          firmware/lib/sync/stage.h
 * @brief         Staged initialization sycnrhonization library
 *
 * This is a library of functions used by application firmware to
 * provide a standard way to order threads and islands
 *
 * The macro SYNC_STAGE_SET_PREINIT should be used in the C source
 * every ME that is to be synchronized. This should be invoked with
 * the number of stages that are used (in the application), the number
 * of contexts used in the microengine, the number of microengines
 * used in the island, and the number of islands used in the
 * application
 *
 * It is then expected that every context in the microengine will
 * eventually invoke sync_stage_set_stage_complete(NUM_STAGES) to
 * indicate that it has completed initialization.
 *
 * Prior to invoking sync_stage_set_stage_complete with NUM_STAGES,
 * the thread may invoke it with a smaller number (still greater than
 * 0). This will permit all threads in the application that wait for
 * that value to synchronize, permitting initialization to occur in
 * stages.
 *
 * For example, threads may set up queues in a first stage, then wait
 * for host interaction to start in a second stage, then enter the
 * main execution phase and invoke the final main application thread
 * code.
 *
 * A thread which only host interaction to start and a main thread may
 * call sync_stage_set_stage_complete(QUEUE_STAGE), then run its host
 * interaction, then issue sync_stage_set_stage_complete(ALL_INIT).
 *
 * A thread which has queues to initialize and no host interaction may
 * perform its queue initialization then just issue
 * sync_stage_set_stage_complete(ALL_INIT).
 * 
 */
#ifndef _SYNC__SYNC_H_
#define _SYNC__SYNC_H_

/** Includes
 */
#include <stdint.h>
#include <nfp.h>

/** struct sync_stage_set_hdr
 */
struct sync_stage_set_hdr
{
    int total_stages;
    int total_users;
    int last_stage_completed;
    int users_completed;
};

/** struct sync_stage_set
 */
struct sync_stage_set
{
    struct sync_stage_set_hdr hdr;
    struct {
        uint64_t queue_lock[4];
    };
};

/** Macros
 */
#define __SYNC_STAGE_SET_PREINIT(num_stages,num_users,scope,mem,ql_init)       \
    __asm { .alloc_mem        scope ## _sync_stage_set_mem mem scope 256 256 } \
    __asm { .declare_resource scope ## _sync_stage_set_res scope 256 scope ## _sync_stage_set_mem }   \
    __asm { .alloc_resource   scope ## _sync_stage_set scope ## _sync_stage_set_res scope 256 }  \
    __asm { .init             scope ## _sync_stage_set+0 num_stages }                  \
    __asm { .init             scope ## _sync_stage_set+4 num_users }                 \
    __asm { .init             scope ## _sync_stage_set+8 0 }                           \
    __asm { .init             scope ## _sync_stage_set+12 0 }                          \
    __asm { .init             scope ## _sync_stage_set+16 ql_init }  /* Start preclaimed */ \
    __asm { .init             scope ## _sync_stage_set+20 0 }                          \
    __asm { .init             scope ## _sync_stage_set+24 0 }                          \
    __asm { .init             scope ## _sync_stage_set+28 0 }                     

#define SYNC_STAGE_SET_GLOBAL_PREINIT(num_stages,num_islands)  \
    __SYNC_STAGE_SET_PREINIT(num_stages,num_islands,global,emem,0)

#define SYNC_STAGE_SET_ISLAND_PREINIT(num_stages,num_mes)      \
    __SYNC_STAGE_SET_PREINIT(num_stages,num_mes,island,ctm,16)

#define SYNC_STAGE_SET_ME_PREINIT(num_stages,num_ctx)      \
    __declspec(shared) int __sss_num_ctx=num_ctx;

#define SYNC_STAGE_SET_PREINIT(num_stages,num_ctxts,num_mes,num_islands) \
    SYNC_STAGE_SET_ME_PREINIT(num_stages,num_ctxts) \
    SYNC_STAGE_SET_ISLAND_PREINIT(num_stages,num_mes) \
    SYNC_STAGE_SET_GLOBAL_PREINIT(num_stages,num_islands)

/** sync_stage_set_stage_complete
 *
 * Indicate that a synchronization stage set is complete. The thread
 * will wait until all threads, MEs, islands synchronize (assuming
 * they are configured correctly) at the end of the specified stage.
 *
 * @param stage Stage that has completed; can be any stage number that
 *              has not already completed
 *
 */
__noinline void sync_state_set_stage_complete(int stage);

/** Close guard
 */
#endif /*_SYNC__SYNC_H_ */
