#include <sys/types.h>
#include <lib_AT91SAM7.h>

#include "../openpcd.h"

void load_mod_level(u_int8_t level)
{
	if (level > 3)
		level = 3;

	if (level & 0x1)
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD1);
	else
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD1);

	if (level & 0x2)
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD2);
	else
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD2);
}

void load_mod_init(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD1);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD2);

	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD1);
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD2);
}
