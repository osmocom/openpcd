#ifndef _OPCD_REG_H
#define _OPCD_REG_H

#include <openpicc.h>
#include <sys/types.h>

#ifdef DEBUG
u_int16_t opicc_reg_read(enum opicc_reg reg);
void opicc_reg_write(enum opicc_reg reg, u_int16_t val);
#else
u_int16_t opicc_regs[_OPICC_NUM_REGS];
#define opicc_reg_read(x)	(opicc_regs[x])
#define opicc_reg_Write(x, y)	(opicc_regs[x] = y)
#endif

void opicc_usbapi_init(void);

#endif
