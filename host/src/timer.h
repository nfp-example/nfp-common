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
 * @file          timer.h
 * @brief         x86 Host timing macros
 *
 * This file supplies a few macros to support high-precision
 * timestamping of host code.
 *
 */

/*a Wrapper
 */
#ifdef __INC_SL_TIMER
#else
#define __INC_SL_TIMER

/*a Includes
 */

/*a Defines
 */
/** The x86 CPU speed effects the correlation between CPU ticks and
 * realtime, which therefore effects performance measurements;
 * SL_TIMER_x86_CLKS_PER_US provides either a compile-time constant
 * (if known at compile time) or whatever CLKS_PER_US is set to.
 */
#ifdef CLKS_PER_US
#define SL_TIMER_x86_CLKS_PER_US (CLKS_PER_US)
#else
#define SL_TIMER_x86_CLKS_PER_US (2400)
#endif

/** GNU C compiler (and compilers that support the GNU C extensions,
 * such as clang/llvm) provide access to assembler to premit reading
 * the CPU timestamp, which is a 64-bit timestamp that has to be
 * assembled to provide a value to a caller. Other compilers might
 * support other versions of inline assembler, but since GNU C
 * extensions are most common (and covers Linux / OSX) this suffices
 **/
#ifdef __GNUC__
#define SL_TIMER_CPU_CLOCKS ({unsigned long long x;unsigned int lo,hi; __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));x = (((unsigned long long)(hi))<<32) | lo;x;})
#else
#define SL_TIMER_CPU_CLOCKS (0)
#endif

/** Convert SL_TIMER_CPU_CLOCKS value (or difference between such
 * values) into a double-prescision microsecond value
 **/
#define SL_TIMER_US_FROM_CLKS(clks) (clks/(0.0+SL_TIMER_x86_CLKS_PER_US))

/** Initialize a timer structure
 */
#define SL_TIMER_INIT(t) {(t).accum_clks=0;(t).last_accum_clks=0;}

/** Mark entry to a patch of code, be it a block or a function;
 * records the entry timestamp in the timer structure
 */
#define SL_TIMER_ENTRY(t) {t.entry_clks = SL_TIMER_CPU_CLOCKS;}

/** Find elapsed timestamp difference between now and the entry time
 * for the timer structure
 */
#define SL_TIMER_ELAPSED(t) (SL_TIMER_CPU_CLOCKS-((t).entry_clks))

/** Mark exit to a patch of code, recording the time spent since the
 * last SL_TIMER_ENTRY, and accumulating total time across all
 * occurences of the patch in the timer structure
 */
#define SL_TIMER_EXIT(t) {unsigned long long now; now = SL_TIMER_CPU_CLOCKS; (t).accum_clks += (now-(t).entry_clks);}

/** Return the total time accumulated in the timer structure over all
 * occcurences.
 */
#define SL_TIMER_VALUE(t) ((t).accum_clks)

/** Return the total time accumulated, in microseconds, in the timer structure over all
 * occcurences.
 */
#define SL_TIMER_VALUE_US(t) (SL_TIMER_US_FROM_CLKS((t).accum_clks))

/** Return the time accumulated in the timer structure since last call
 * of SL_TIMER_DELTA_VALUE
 */
#define SL_TIMER_DELTA_VALUE(t) ({unsigned long long r; r = (t).accum_clks - (t).last_accum_clks; (t).last_accum_clks=(t).accum_clks;r;})

/** Return the time, in microseconds, accumulated in the timer
 * structure since last call of SL_TIMER_DELTA_VALUE
 */
#define SL_TIMER_DELTA_VALUE_US(t) ({unsigned long long r;r=SL_TIMER_DELTA_VALUE(t);SL_TIMER_US_FROM_CLKS(r);})

/*a Types
 */
/*t t_sl_timer */
/** Structure to contain the timestamps required
 */
typedef struct t_sl_timer
{
    /** Timestamp at last SL_TIMER_ENTRY **/
    unsigned long long int entry_clks;

    /** Timestamp differences accumulated across every SL_TIMER_ENTRY
     * to SL_TIMER_EXIT pair **/
    unsigned long long int accum_clks;

    /** Last value of accum_clks when SL_TIMER_DELTA_VALUE was last
     * called **/
    unsigned long long int last_accum_clks;
} t_sl_timer;

/*a External functions
 */

/*a Wrapper
 */
#endif


/*a Editor preferences and notes
mode: c ***
c-basic-offset: 4 ***
c-default-style: (quote ((c-mode . "k&r") (c++-mode . "k&r"))) ***
outline-regexp: "/\\\*a\\\|[\t ]*\/\\\*[b-z][\t ]" ***
*/
