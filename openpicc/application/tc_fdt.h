#ifndef _TC_FDT_H
#define _TC_FDT_H

#include <sys/types.h>

extern void tc_fdt_init(void);
extern void tc_fdt_set(u_int16_t count);
extern void __ramfunc tc_fdt_set_to_next_slot(int last_bit);

#endif
