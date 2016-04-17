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
 * @file          firmware/lib/sync/_c/stage.c
 * @brief         Staged initialization sycnrhonization library
 *
 * This is a library of functions used by application firmware to
 * provide a standard way to order threads and islands
 * 
 */

/** Includes
 */
#include <sync/stage.h>
#include <nfp_override.h>
#include <nfp/mem.h>

/** Debug definitions
    mailbox0 = SS0W00NN; S = last stage completed by context, W=0->not last context of ME, 1->last context of ME synchronizing, 2-> last context and synced, N=number of contexts done
    mailbox1 = SS0W00NN; S = last stage completed by all MEs in island, W=0->not last ME of island, 1->last ME if island synchronizing, 2-> last ME and synced, N=MES completed
    mailbox2 = SS0W00NN; S = last stage completed by context as last ME in island, W=0->not last island, 1->last island if island synchronizing, 2-> last island and synced
 */
#define DEBUG_ISLAND_COMPLETE(stage,hdr) do { \
    register int i=(hdr.last_stage_completed<<24); \
    i |= hdr.users_completed; \
    local_csr_write(local_csr_mailbox2, i); \
    i=local_csr_read(local_csr_mailbox3); \
    i+=(1<<16);                            \
    local_csr_write(local_csr_mailbox3, i); \
    } while (0)
#define DEBUG_ISLAND_LAST_COMPLETE(stage,hdr) do { \
    register int i=local_csr_read(local_csr_mailbox2); \
    local_csr_write(local_csr_mailbox2, (1<<16) | i);  \
    } while (0)
#define DEBUG_ISLAND_NEW_STAGE(stage,hdr) do { \
    register int i=(hdr.last_stage_completed<<24); \
          local_csr_write(local_csr_mailbox2, (2<<16) | i); \
    } while (0)

#define DEBUG_ME_COMPLETE(hdr) do { \
    register int i=(hdr.last_stage_completed<<24); \
    i |= hdr.users_completed; \
    local_csr_write(local_csr_mailbox1, i); \
    i=local_csr_read(local_csr_mailbox3); \
    i++; \
    local_csr_write(local_csr_mailbox3, i); \
    } while (0)
#define DEBUG_ME_COMPLETE_LAST(hdr) do { \
    int i=local_csr_read(local_csr_mailbox1); \
    local_csr_write(local_csr_mailbox1,(1<<16) | i); \
    } while (0)
#define DEBUG_ME_NEW_STAGE(hdr) do { \
    register int i=(hdr.last_stage_completed<<24); \
          local_csr_write(local_csr_mailbox1, (2<<16) | i); \
    } while (0)

#define DEBUG_CTX_COMPLETE(stage) do {        \
    int i=local_csr_read(local_csr_mailbox0); \
    i=(i+1) &~ (0xffff<<16);\
    i |= (stage<<24); \
    local_csr_write(local_csr_mailbox0,i); \
    if (last_stage_completed==0) local_csr_write(local_csr_mailbox3,0); \
    } while (0)
#define DEBUG_ALL_CTXS_COMPLETE(stage) do { \
    int i=local_csr_read(local_csr_mailbox0); \
    local_csr_write(local_csr_mailbox0,(1<<16)|i);   \
    } while (0)
#define DEBUG_NO_CTXS_COMPLETE(stage) do { \
    int i=local_csr_read(local_csr_mailbox0); \
    local_csr_write(local_csr_mailbox0,(2<<16)|i);   \
    } while (0)


/** Static variables
 */
__declspec(shared) int __sss_num_ctx;
static __declspec(shared) int num_ctx_done;
static __declspec(shared) int next_sig_ctx;
static __declspec(shared) int last_stage_completed;

/** encode_signal
 *
 * Encode a signal for a thread of an ME in an island into a 32-bit
 * value, for placing on a MicroQ. Requires an EVEN signal number,
 * hence use of a SIGNAL_PAIR.
 *
 * @param sig  SIGNAL_PAIR, the even signal of which is encoded
 *
 */
static __intrinsic int
encode_signal(SIGNAL_PAIR *sig)
{
    uint32_t ctxsts;
    int island_id, me_id, ctx, sig_num;

    ctxsts = local_csr_read(local_csr_active_ctx_sts);
    island_id = (ctxsts >> 25) & 0x3f;
    me_id     = (ctxsts >> 3) & 0xf;
    ctx       = (ctxsts >> 0) & 0x7;
    sig_num   = __signal_number(&sig->even) >> 1;
    return ( (island_id << 10) |
             (me_id << 6) | 
             (ctx << 3) |
             (sig_num << 0) );
}

/** send_signal
 *
 * Signal an encoded signal using CTM interthread signal
 *
 * @param encoded_signal  Encoded signal (from encode_signal) to hit
 *
 */
static __intrinsic void
send_signal(uint32_t encoded_signal)
{
    uint32_t addr;
    int island_id, me_id, ctx, sig_num;
    island_id = (encoded_signal >> 10) & 0x3f;
    me_id     = (encoded_signal >> 6)  & 0xf;
    ctx       = (encoded_signal >> 3)  & 0x7;
    sig_num   = (encoded_signal >> 0)  & 0x7;
    addr = ( (island_id << 24) |
             (me_id << 9) |
             (ctx << 6) |
             (sig_num << 3) );
    __asm {
        ct[interthread_signal, --, addr, 0, 1];
    }
}

/** send_signal_cls_reflect
 *
 * Signal an encoded signal using CLS reflect (uses CLS in remote island)
 *
 * @param encoded_signal  Encoded signal (from encode_signal) to hit
 *
 */
static __intrinsic void
send_signal_cls_reflect(uint32_t encoded_signal)
{
    uint32_t addr;
    int island_id, me_id, ctx, sig_num;
    SIGNAL sig;
    __xread uint32_t unused_data;
    island_id = (encoded_signal >> 10) & 0x3f;
    me_id     = (encoded_signal >> 6)  & 0xf;
    ctx       = (encoded_signal >> 3)  & 0x7;
    sig_num   = (encoded_signal >> 0)  & 0x7;
    addr = ( (island_id << (34 - 8)) |
             (1 << (31 - 8)) |
             (ctx << (28 - 8)) |
             (sig_num << (25 - 8)) |
             (me_id << (12 - 8)) );
    __asm {
        cls[reflect_to_sig_both, unused_data, addr, <<8, 0, 1],\
            ctx_swap[sig];
    }
}

/** mes_in_island_complete
 * 
 * A microQ is used to queue up signals of islands that have completed a
 * stage. Also a count of 'completed islands' is maintained.
 *
 * When an island completes a stage it first adds an entry to the
 * microQ with a 'restart signal', which is an even signal number that
 * the thread will wait for. It then increments the 'num islands that
 * have completed the stage'. It is the last island to complete the
 * stage if the pre-increment value is total islands-1.
 *
 * If the island IS the last to complete the stage then it is
 * responsible for signaling the waiting threads. It has to Get from
 * the MicroQ 'total islands' times, and each time it must signal the
 * thread as indicated by the data from the microq.
 *
 * All islands then wait for their 'restart signal'. The 'last island'
 * will signall all the threads, including itself, and so the islands
 * will all move on.
 *
 * The MicroQ must start empty, and will always have at most
 * total_users on it. Therefore total_users must not exceed 14, as
 * that is the longest a MicroQ can get.
 *
 */
static void
mes_in_island_complete(__mem struct sync_stage_set *global_sync_stage_set,
                       int stage)
{
    __xread struct sync_stage_set_hdr hdr;
    uint32_t gsss_base;

    gsss_base = (uint32_t) (((uint64_t)global_sync_stage_set) >> 8);
    mem_atomic_read_s8(&hdr, gsss_base, 0, sizeof(hdr) );
    if (stage < hdr.last_stage_completed) {
        __asm { ctx_arb[bpt] }
    }

    while (stage > hdr.last_stage_completed) {
        SIGNAL_PAIR ql_sig;
        SIGNAL_PAIR ta_sig;
        __xrw uint32_t data[3];
        int MQ_OFS, UC_OFS, UCM_OFS, LSC_OFS;

        MQ_OFS = offsetof(struct sync_stage_set,queue_lock);
        UC_OFS = offsetof(struct sync_stage_set,hdr.users_completed);
        UCM_OFS = offsetof(struct sync_stage_set,hdr.users_completed_mask);
        LSC_OFS = offsetof(struct sync_stage_set,hdr.last_stage_completed);

        /* Add to the queue lock list of waiting users, increment
         * users completed count
         */
        data[0] = encode_signal(&ql_sig);
        data[1] = 1;
        {
            uint32_t ctxsts, island_id;
            ctxsts = local_csr_read(local_csr_active_ctx_sts);
            island_id = (ctxsts >> 25) & 0x3f;
            island_id = ((island_id & 0x30)>>1) | (island_id & 0xf);
            data[2] = 1<<island_id;
        }
        __asm {
            mem[microq256_put, data[0], gsss_base, <<8, MQ_OFS];
            mem[set, data[2], gsss_base, <<8, UCM_OFS, 1];
            mem[test_and_add, data[1], gsss_base, <<8, UC_OFS, 1], \
                sig_done[ta_sig];            
        }
        wait_for_all(&ta_sig);

        /* If was last to add to list, then update structure, then
         * signal all threads on the list
        */
        DEBUG_ISLAND_COMPLETE(stage,hdr);
        if (data[1] + 1 == hdr.total_users) {
            int i;
            DEBUG_ISLAND_LAST_COMPLETE(stage,hdr);
            data[0]=0;
            __asm {
                mem[incr, --, gsss_base, <<8, LSC_OFS];
                mem[atomic_write, data[0], gsss_base, <<8, UCM_OFS, 1];
                mem[atomic_write, data[0], gsss_base, <<8, UC_OFS, 1];
            }
            for (i=0; i<hdr.total_users; i++) {
                SIGNAL_PAIR mg_sig;
                __asm {
                    mem[microq256_get, data[0], gsss_base, <<8, MQ_OFS], \
                        sig_done[mg_sig];
                    ctx_arb[mg_sig.even];
                }
                send_signal(data[0]);
            }
        }

        /* Wait to be signaled
         */
        __asm {
            ctx_arb[ql_sig.even];
        }
        //__implicit_undef(ql_sig.odd);

        /* Reread the header to get up-to-date last_stage_completed etc
         */
        mem_atomic_read_s8(&hdr, gsss_base, 0, sizeof(hdr) );
    }
    DEBUG_ISLAND_NEW_STAGE(stage,hdr);
}

/** contexts_in_me_complete
 * 
 * A queue lock is used to synchronize MEs that have reached the
 * end of the current stage The queue lock is notionally 'preclaimed'
 * by the LAST ME to complete the stage.
 *
 * When an ME completes a stage it first posts a claim of the queue
 * lock, then it increments the 'number of MEs that have completed the
 * stage' And it reads the 'total MEs' too. It is the last ME
 * to complete the stage if the pre-increment value is total
 * total_users-1.
 *
 * If the ME IS the last to complete the stage then it claims the
 * lock again but with a null signal (the preclaim for the next
 * stage), and it releases the queue lock (the preclaim of the last
 * stage).
 *
 * All MEs then wait for their synchronizing claim of the queue lock
 * to complete, whereupon they immediately release the lock and move
 * on to the next stage.
 *
 * The queue lock therefore must start preclaimed (value 16). Every ME
 * involved in the synchronization will be queued on the lock, plus
 * the 'null preclaim' for the next stage. A queue lock can have at
 * most 14 pending claimants, so at most 13 MEs can be involved in a
 * stage set (total_users<=13). Also, queue locks only work if the
 * claimants are in the same island (in NFP6xxx); so if at most one ME
 * per island uses this function then the operation is safe (there are
 * at most 12 MEs in an island).
 *
 */
static void
contexts_in_me_complete(void)
{
    SIGNAL_PAIR ql_sig;
    SIGNAL_PAIR ta_sig;
    __xrw uint32_t data[2];
    int QL_OFS, UC_OFS, UCM_OFS, LSC_OFS;
    __mem struct sync_stage_set *island_sync_stage_set;
    __xread struct sync_stage_set_hdr hdr;

    uint32_t isss_base;
    island_sync_stage_set = (__mem struct sync_stage_set *)__link_sym("island_sync_stage_set");

    isss_base = (uint32_t) (((uint64_t)island_sync_stage_set) >> 8);
    mem_atomic_read_s8(&hdr, isss_base, 0, sizeof(hdr) );

    QL_OFS  = offsetof(struct sync_stage_set, queue_lock);
    UC_OFS  = offsetof(struct sync_stage_set, hdr.users_completed);
    UCM_OFS = offsetof(struct sync_stage_set, hdr.users_completed_mask);
    LSC_OFS = offsetof(struct sync_stage_set, hdr.last_stage_completed);

    /* Add to the queue lock list of users waiting, and increment number
     */
    data[0] = 1;
    {
        uint32_t ctxsts, me_id;
        ctxsts = local_csr_read(local_csr_active_ctx_sts);
        me_id     = (ctxsts >> 3) & 0xf;
        data[1] = 1<<me_id;
    }
    me_clear_all_signals();
    __asm {
        mem[queue256_lock, --, isss_base, <<8, QL_OFS], \
            sig_done[ql_sig];
        mem[set,          data[1], isss_base, <<8, UCM_OFS, 1];
        mem[test_and_add, data[0], isss_base, <<8, UC_OFS, 1], \
            sig_done[ta_sig];            
    }
    wait_for_all(&ta_sig);

    /* If was last to add to list, then add 'last' claimant, update structure, and...
       sync with island level and ...
       unlock the chain
    */
    DEBUG_ME_COMPLETE(hdr);
    if (data[0] + 1 == hdr.total_users) {
        SIGNAL sig;
        SIGNAL_PAIR last_sig;
        __mem struct sync_stage_set *global_sync_stage_set;

        DEBUG_ME_COMPLETE_LAST(hdr);
        data[0]=0;
        __asm {
            mem[queue256_lock, --, isss_base, <<8, QL_OFS], \
                sig_done[last_sig];
            mem[incr, --, isss_base, <<8, LSC_OFS];
            mem[atomic_write, data[0], isss_base, <<8, UCM_OFS, 1];
            //mem[atomic_write, data[0], isss_base, <<8, UC_OFS, 1],   \
                  //    sig_done[sig];
            mem[atomic_write, data[0], isss_base, <<8, UC_OFS, 1];
            mem[atomic_read, data[0], isss_base, <<8, UC_OFS, 1], sig_done[sig];
        }
        wait_for_all(&sig);

        global_sync_stage_set = (__mem struct sync_stage_set *)__link_sym("global_sync_stage_set");
        mes_in_island_complete(global_sync_stage_set,
                               hdr.last_stage_completed + 1);
        __asm {
            mem[queue256_unlock, --, isss_base, <<8, QL_OFS, 0];
            ctx_arb[ql_sig.even];
            mem[queue256_unlock, --, isss_base, <<8, QL_OFS, 0];
            ctx_arb[last_sig.even];
        }
    } else {
        /* Not the last so wait to be unlocked from the list, and
         * unlock the next
         */
        __asm {
            ctx_arb[ql_sig.even];
            mem[queue256_unlock, --, isss_base, <<8, QL_OFS, 0];
        }
    }
    DEBUG_ME_NEW_STAGE(hdr);
}

/** issue_sequence_signal
 *
 * Isssue signal to same ME, and ctx_arb to let things run
 *
 * @param sig_ctx  Signal/context to signal
 *
 */
static __intrinsic void
issue_sequence_signal(int sig_ctx)
{
    local_csr_write(local_csr_same_me_signal, sig_ctx);
    __asm { ctx_arb[voluntary] }
}

/** get_sequence_signal
 *
 * Get a sig_ctx for this context, and a given signal
 *
 * @param sig  Signal to get value for
 *
 */
static __intrinsic int
get_sequence_signal(volatile SIGNAL *sig)
{
    uint32_t sig_ctx;
    sig_ctx = (__signal_number(sig) << 3) | ctx();
    return sig_ctx;
}

/** context_complete
 *
 * Handle the completion of a stage for a context, and return the stage completed.
 *
 * @param total_ctx Total number of contexts running in the microengine
 *
 */
static int
context_complete(int total_ctx)
{
    int stage_completed;
    int sig_ctx_to_signal;

    stage_completed = last_stage_completed + 1;
    num_ctx_done += 1;
    DEBUG_CTX_COMPLETE(last_stage_completed);
    if (num_ctx_done < total_ctx) {
        SIGNAL sig;
        sig_ctx_to_signal = next_sig_ctx;
        next_sig_ctx = get_sequence_signal(&sig);
        wait_for_all(&sig);
    } else {
        sig_ctx_to_signal = next_sig_ctx;
        num_ctx_done = 0;
        next_sig_ctx = 0;
        last_stage_completed++;
        DEBUG_ALL_CTXS_COMPLETE(stage_completed);
        contexts_in_me_complete();
        DEBUG_NO_CTXS_COMPLETE(stage_completed);
    }
    if (sig_ctx_to_signal != 0) {
        issue_sequence_signal(sig_ctx_to_signal);
    }
    return stage_completed;
}

/** sync_stage_set_stage_complete
 * 
 * Use a sync_stage_set in an island to synchronize the MEs in the island.
 * Couple with a sync_stage_set globally to synchronize the islands
 */
__noinline void
sync_state_set_stage_complete(int stage)
{
    if (stage <= last_stage_completed) {
        __asm { ctx_arb[bpt] }
    }
    for (;;) {
        if (stage == context_complete(__sss_num_ctx)) break;
    }
}


