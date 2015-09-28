#ifndef _OPCD_REG_H
#define _OPCD_REG_H

#include <openpicc.h>
#include <sys/types.h>

#ifdef DEBUG
uint16_t opicc_reg_read(enum opicc_reg reg);
void opicc_reg_write(enum opicc_reg reg, uint16_t val);
#else
uint16_t opicc_regs[_OPICC_NUM_REGS];
#define opicc_reg_read(x)	(opicc_regs[x])
#define opicc_reg_write(x, y)	(opicc_regs[x] = y)
#endif

void opicc_usbapi_init(void);

#endif
