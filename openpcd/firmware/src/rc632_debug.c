
#include "rc632.h"
#include "dbgu.h"
#include <include/cl_rc632.h>

#ifdef DEBUG
static int rc632_reg_write_verify(u_int8_t reg, u_int8_t val)
{
	u_int8_t tmp;

	rc632_reg_write(reg, val);
	tmp = rc632_reg_read(reg);

	DEBUGP("reg=0x%02x, write=0x%02x, read=0x%02x ", reg, val, tmp);

	return (val == tmp);
}

int rc632_test(void)
{
	if (rc632_reg_write_verify(RC632_REG_MOD_WIDTH, 0x55) != 1)
		return -1;

	if (rc632_reg_write_verify(RC632_REG_MOD_WIDTH, 0xAA) != 1)
		return -1;

	return 0;
}

#endif
