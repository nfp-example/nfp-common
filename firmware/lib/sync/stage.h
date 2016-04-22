/*a Copyright */
/**
 * Copyright (C) 2015-2016,  Gavin J Stark.  All rights reserved.
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

/*a Opening guard */
#ifndef _SYNC__SYNC_H_
#define _SYNC__SYNC_H_

/*a Includes */
#include <stdint.h>
#include <nfp.h>

/*a Types */
/*t struct sync_stage_set_hdr - CHANGE PREINIT MACRO IF YOU CHANGE THIS */
/**
 * Structure used to keep up to date with which MEs/islands have
 * completed the current stage, and how many MEs/islands there are to
 * synchronize
 *
 * One instance of this structure is used per island, and one for the
 * device
 *
 */
struct sync_stage_set_hdr
{
    /** Total number of initialization stages to synchonize **/
    int total_stages;
    /** Total number of MEs/islands being synchronized by this
     * structure **/
    int total_users;
    /** Last initialization stage completed by all of the users of
     * this structure **/
    int last_stage_completed;
    /** Number of users that have completed the next stage
     * (last_stage_completed+1) **/
    int users_completed;
    /** Mask of which users have completed the next stage - for debug
     * purposes only, when things hang... **/
    int users_completed_mask;
    /** Padding to force to 32-B alignment, to permit atomic
     * operations on the _next_ structure this is used with **/
    int padding[3];
};

/*t struct sync_stage_set - CHANGE PREINIT MACRO IF YOU CHANGE THIS */
/**
 * Structure used to keep track of which MEs/islands have completed
 * which initialization stage, plus locking structures for the MEs
 * that have completed and are waiting for others to complete.
 */
struct sync_stage_set
{
    /** Header structure keeping track of the number of users and
     * which stage is being tracked.
     *
     * MUST BE A MULTIPLE OF 16B
     *
     **/
    struct sync_stage_set_hdr hdr;
    /** Structure ALIGNED TO 16B that keeps track of pending users,
     * that have completed the initialization stage and are waiting
     * for the last one across the line to signal them
     *
     * MUST BE ALIGNED TO 32B boundary
     *
     * **/
    struct {
        /** Queue of locks memory, managed by an atomic memory unit
        engine, which keeps the list of waiting users. This must all
        fit within a 64B cache line, and it must be aligned to a 16B
        boundary - and to support >8 users it must be 32B long. **/
        uint64_t queue_lock[4];
    };
};

/*a Macros
 */
/**
 * For internal use
 *
 * Declare and initialize memory structure used in a CTM or global
 * memory for managing multiple MEs/islands synchronization.
 *
 * Exposed to user code with SYNC_STAGE_SET_PREINIT and
 *
 * A struct sync_stage_set will be placed in @p mem visible with @p
 * scope (island or global).
 *
 * The structure should start with the queue lock preclaimed (which
 * differs depending on whether it is for MEs or for islands), so @p
 * ql_init should be 1 for MEs.
 *
 * Note that was:
 * @code{.c}
 *  __asm { .alloc_mem        scope ## _sync_stage_set_mem mem scope 256 256 } \
 *  __asm { .declare_resource scope ## _sync_stage_set_res scope 256 scope ## _sync_stage_set_mem } \
 *  __asm { .alloc_resource   scope ## _sync_stage_set scope ## _sync_stage_set_res scope 256 } \
 * @endcode
 *
 * but that broke in SDK 6.0 alpha
 *
 **/
#define __SYNC_STAGE_SET_PREINIT(num_stages,num_users,scope,mem,ql_init) \
    __asm { .alloc_mem        scope ## _sync_stage_set mem scope 256 256 } \
    __asm { .init             scope ## _sync_stage_set+0 num_stages }   \
    __asm { .init             scope ## _sync_stage_set+4 num_users }    \
    __asm { .init             scope ## _sync_stage_set+8 0 }            \
    __asm { .init             scope ## _sync_stage_set+12 0 }           \
    __asm { .init             scope ## _sync_stage_set+16 0 }           \
    __asm { .init             scope ## _sync_stage_set+20 0 }           \
    __asm { .init             scope ## _sync_stage_set+24 0 }           \
    __asm { .init             scope ## _sync_stage_set+28 0 }           \
    __asm { .init             scope ## _sync_stage_set+32 ql_init }  /* Start preclaimed */ \
    __asm { .init             scope ## _sync_stage_set+36 0 }           \
    __asm { .init             scope ## _sync_stage_set+40 0 }           \
    __asm { .init             scope ## _sync_stage_set+44 0 }           \
    __asm { .init             scope ## _sync_stage_set+48 0 }           \
    __asm { .init             scope ## _sync_stage_set+52 0 }           \
    __asm { .init             scope ## _sync_stage_set+56 0 }           \
    __asm { .init             scope ## _sync_stage_set+60 0 }

#define __SYNC_STAGE_SET_PREINIT_EXTERN(scope) \
    extern __mem int scope ## _sync_stage_set;

/**
 * Internal use only
 *
 * Declare and initialize memory structure used in a global memory for
 * managing multiple islands synchronization.
 **/
#define __SYNC_STAGE_SET_GLOBAL_PREINIT(num_stages,num_islands)       \
    __SYNC_STAGE_SET_PREINIT(num_stages,num_islands,global,emem,0)

/**
 * Internal use only
 *
 * Declare and initialize memory structure used in an island memory for
 * managing multiple MEs synchronization.
 **/
#define __SYNC_STAGE_SET_ISLAND_PREINIT(num_stages,num_mes)       \
    __SYNC_STAGE_SET_PREINIT(num_stages,num_mes,island,ctm,16)

/**
 * Internal use only
 *
 * Declare and initialize memory structure used for synchronizing
 * contexts within a microengine
 **/
#define __SYNC_STAGE_SET_ME_PREINIT(num_ctx)        \
    __declspec(shared) int __sss_num_ctx=num_ctx;

/**
 * Declare and initialize memory structures used for synchronizing
 * contexts, microengines, and islands - at least one ME per islands
 * must have this in its code
 **/
#define SYNC_STAGE_SET_PREINIT(num_stages,num_ctxts,num_mes,num_islands) \
    __SYNC_STAGE_SET_ME_PREINIT(num_ctxts)                              \
    __SYNC_STAGE_SET_ISLAND_PREINIT(num_stages,num_mes)                   \
    __SYNC_STAGE_SET_GLOBAL_PREINIT(num_stages,num_islands)

/**
 * Declare and initialize memory structures used for synchronizing just
 * contexts - can br 
 **/
#define SYNC_STAGE_SET_PREINIT_ME(num_ctxts) \
    __SYNC_STAGE_SET_ME_PREINIT(num_ctxts)     \
    __SYNC_STAGE_SET_PREINIT_EXTERN(island) \
    __SYNC_STAGE_SET_PREINIT_EXTERN(global)

/*f sync_stage_set_stage_complete */
/**
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

/*a Close guard */
#endif /*_SYNC__SYNC_H_ */
