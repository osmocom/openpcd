/* PLL routines for OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include <os/pio_irq.h>
#include <os/dbgu.h>
#include <os/led.h>
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

static void pll_lock_change_cb(uint32_t pio)
{
	DEBUGPCRF("PLL LOCK: %d", pll_is_locked());
#if 1
	if (pll_is_locked())
		led_switch(2, 1);
	else
		led_switch(2, 0);
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
