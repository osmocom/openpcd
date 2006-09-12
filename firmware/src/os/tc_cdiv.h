#ifndef _TC_CDIV_H
#define _TC_CDIV_H

#include <sys/types.h>
#include <lib_AT91SAM7.h>

static AT91PS_TCB tcb;

extern void tc_cdiv_phase_add(int16_t inc);
extern void tc_cdiv_set_divider(u_int16_t div);

static inline void tc_cdiv_phase_inc(void)
{
       tc_cdiv_phase_add(1);
}

static inline void tc_cdiv_phase_dec(void)
{
       tc_cdiv_phase_add(-1);
}

extern void tc_cdiv_print(void);
extern void tc_cdiv_init(void);
extern void tc_cdiv_fini(void);

#endif
