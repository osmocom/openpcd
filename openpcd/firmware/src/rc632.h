#ifndef _RC623_API_H
#define _RC632_API_H

#include <include/types.h>
#include <include/cl_rc632.h>

extern void rc632_write_reg(u_int8_t addr, u_int8_t data);
extern void rc632_write_fifo(u_int8_t len, u_int8_t *data);
extern u_int8_t rc632_read_reg(u_int8_t addr);
extern u_int8_t rc632_read_fifo(u_int8_t max_len, u_int8_t *data);
extern void rc632_init(void);
extern void rc632_exit(void);

#ifdef DEBUG
extern int rc632_test(void);
extern int rc632_dump(void);
#endif

#endif
