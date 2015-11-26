/*
 * Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 * @file          firmware/lib/nfp/_c/me.c
 * @brief         Microengine assist library functions
 *
 * This is a library of functions used by application firmware to
 * utilize various ME features in a uniform manner
 * 
 */

/** Includes
 */
#include <nfp/me.h>
#include <stdint.h>
#include <nfp.h>

/** me_clear_all_signals
 * Clear all ME signals
 */
__intrinsic void me_clear_all_signals(void)
{
    local_csr_write(local_csr_active_ctx_sig_events, 0);
}

/** me_poll_wait_for_sig_with_timeout
 */
int me_poll_wait_for_sig_with_timeout(int signal_num, int timeout)
{
    uint32_t ts_in, ts_lp;
    uint32_t signal_mask;

    ts_lp = ts_in = local_csr_read(local_csr_timestamp_low);
    while (ts_lp-ts_in<(timeout>>4)) {
        signal_mask = local_csr_read(local_csr_active_ctx_sig_events);
        if ((signal_mask>>signal_num)&1) {
            return 1;
        }
        ts_lp = local_csr_read(local_csr_timestamp_low);
    }
    return 0;
}

/** me_poll_sleep
 * Wait for a specified number of ME clock ticks by busy polling
 * Must only be used where context swapping is not permitted or where
 * signal use is not permitted
 *
 * @param: timeout
 */
__intrinsic void me_poll_sleep(unsigned int cycles)
{
    uint32_t ts_in, ts_lp;

    ts_lp = ts_in = local_csr_read(local_csr_timestamp_low);
    while (ts_lp-ts_in<(cycles>>4)) {
        ts_lp = local_csr_read(local_csr_timestamp_low);
    }
}

/** me_sleep
 */
__intrinsic void
me_sleep(unsigned int cycles)
{
    uint32_t ts_in, ts_to;
    uint32_t sig_num;
    SIGNAL sig;

    ts_in = local_csr_read(local_csr_timestamp_low);
    ts_to = ts_in+(cycles>>4);

    __implicit_write(&sig);
    sig_num = __signal_number(&sig);
    local_csr_write(local_csr_active_ctx_future_count, ts_to);
    local_csr_write(local_csr_active_future_count_signal, sig_num);
    wait_for_all(&sig);
}

/** me_time64
 *
 * Get 64-bit current time
 *
 */
__intrinsic uint64_32_t
me_time64(void)
{
    uint64_32_t ts;
    ts.uint32_lo = local_csr_read(local_csr_timestamp_low);
    ts.uint32_hi = local_csr_read(local_csr_timestamp_high);
    return ts;
}
