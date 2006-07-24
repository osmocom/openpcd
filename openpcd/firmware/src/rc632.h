#ifndef _RC623_API_H
#define _RC632_API_H

#include <sys/types.h>
#include <include/cl_rc632.h>

extern void rc632_reg_write(u_int8_t addr, u_int8_t data);
extern void rc632_fifo_write(u_int8_t len, u_int8_t *data);
extern u_int8_t rc632_reg_read(u_int8_t addr);
extern u_int8_t rc632_fifo_read(u_int8_t max_len, u_int8_t *data);
extern u_int8_t rc632_clear_bits(u_int8_t reg, u_int8_t bits);
extern u_int8_t rc632_set_bits(u_int8_t reg, u_int8_t bits);
extern void rc632_init(void);
extern void rc632_exit(void);

#ifdef DEBUG
extern int rc632_test(void);
extern int rc632_dump(void);
#endif

#endif
