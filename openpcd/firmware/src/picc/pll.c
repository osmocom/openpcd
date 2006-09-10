
#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include "../openpcd.h"

void pll_inhibit(int inhibit)
{
	if (inhibit)
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_INHIBIT);
	else
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_INHIBIT);
}

int pll_is_locked(void)
{
	return AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_LOCK);
}

void pll_init(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_INHIBIT);
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_LOCK);
	pll_inhibit(0);
}
