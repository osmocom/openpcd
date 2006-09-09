#ifndef _TC_FDT_H
#define _TC_FDT_H

#include <sys/types.h>
#include <lib_AT91SAM7.h>

extern AT91PS_TCB tcb;
extern void tc_fdt_init(void);
extern void tc_fdt_set(u_int16_t count);

#endif
