/*a Copyright

  This file 'sl_timer.h' copyright Gavin J Stark 2003, 2004

  This is free software; you can redistribute it and/or modify it however you wish,
  with no obligations

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.
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
#ifdef CLKS_PER_US
#define SL_TIMER_x86_CLKS_PER_US (CLKS_PER_US)
#else
#define SL_TIMER_x86_CLKS_PER_US (2400)
#endif

#ifdef __GNUC__
#define SL_TIMER_CPU_CLOCKS ({unsigned long long x;unsigned int lo,hi; __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));x = (((unsigned long long)(hi))<<32) | lo;x;})
#else
#define SL_TIMER_CPU_CLOCKS (0)
#endif

#define SL_TIMER_US_FROM_CLKS(clks) (clks/(0.0+SL_TIMER_x86_CLKS_PER_US))

#define SL_TIMER_INIT(t) {(t).accum_clks=0;(t).last_accum_clks=0;}
#define SL_TIMER_ENTRY(t) {t.entry_clks = SL_TIMER_CPU_CLOCKS;}
#define SL_TIMER_ELAPSED(t) (SL_TIMER_CPU_CLOCKS-((t).entry_clks))
#define SL_TIMER_EXIT(t) {unsigned long long now; now = SL_TIMER_CPU_CLOCKS; (t).accum_clks += (now-(t).entry_clks);}
#define SL_TIMER_VALUE(t) ((t).accum_clks)
#define SL_TIMER_VALUE_US(t) (SL_TIMER_US_FROM_CLKS((t).accum_clks))
#define SL_TIMER_DELTA_VALUE(t) ({unsigned long long r; r = (t).accum_clks - (t).last_accum_clks; (t).last_accum_clks=(t).accum_clks;r;})
#define SL_TIMER_DELTA_VALUE_US(t) ({unsigned long long r;r=SL_TIMER_DELTA_VALUE(t);SL_TIMER_US_FROM_CLKS(r);})

/*a Types
 */

/*t t_sl_timer
 */
typedef struct t_sl_timer
{
    unsigned long long int entry_clks;
    unsigned long long int accum_clks;
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
