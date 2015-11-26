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
 * @file          firmware/lib/nfp/me.h
 * @brief         Microengine assist library functions and defines
 *
 * This is a library of functions used by application firmware to
 * utilize various ME features in a uniform manner
 * 
 */
#ifndef _NFP__ME_H_
#define _NFP__ME_H_

/** Includes required
 */
#include "types.h"

/** me_clear_all_signals
 *
 * Clear all the signals for a context
 *
 */
__intrinsic void me_clear_all_signals(void);

/** me_poll_wait_for_sig_with_timeout
 *
 * Wait for a signal without ctx_arb, with a timeout. Used rarely in
 * tightly controlled circumstances. Returns 0 on timeout, 1 if signal
 * fires (signal is NOT cleared).
 *
 * @param signal_num   Signal number of signal to wait for
 * @param timeout      Number of cycles to wait
 *
 */
__intrinsic int me_poll_wait_for_sig_with_timeout(int signal_num, int timeout);

/** me_poll_sleep
 *
 * Wait for a number of cycles without ctx_arb. Used rarely in tightly
 * controlled circumstances
 *
 * @param timeout      Number of cycles to wait
 *
 */
__intrinsic void me_poll_sleep(unsigned int cycles);

/** me_sleep
 *
 * Sleep, waiting for a number of cycles.
 *
 * @param timeout      Number of cycles to wait
 *
 */
__intrinsic void me_sleep(unsigned int cycles);

/** me_time64
 *
 * Get 64-bit current time
 *
 */
__intrinsic uint64_32_t me_time64(void);

/** Close guard
 */
#endif /*_NFP__ME_H_ */
