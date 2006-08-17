#ifndef _TC_H
#define _TC_H

#include <sys/types.h>

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

#ifdef CONFIG_PICCSIM
extern void tc_fdt_set(u_int16_t count);
#endif

#endif
