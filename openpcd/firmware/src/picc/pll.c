
#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include <os/pio_irq.h>
#include <os/dbgu.h>
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

static void pll_lock_change_cb(u_int32_t pio)
{
	DEBUGPCRF("PLL LOCK: %d", pll_is_locked());
#if 1
	if (pll_is_locked())
		led_switch(1, 1);
	else
		led_switch(1, 0);
#endif
}

void pll_init(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_INHIBIT);
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_LOCK);
	pll_inhibit(0);

	pio_irq_register(OPENPICC_PIO_PLL_LOCK, &pll_lock_change_cb);
	pio_irq_enable(OPENPICC_PIO_PLL_LOCK);
}
