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
#include "pll.h"
#include "pio_irq.h"
#include "dbgu.h"
#include "led.h"
#include "board.h"

void pll_inhibit(int inhibit)
{
	if (inhibit)
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_INHIBIT);
	else
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_INHIBIT);
}

int pll_is_inhibited(void)
{
	return AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_INHIBIT);
}

int pll_is_locked(void)
{
	return AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC->PLL_LOCK);
}

static void pll_lock_change_cb(u_int32_t pio)
{
	(void)pio;
	DEBUGPCRF("PLL LOCK: %d", pll_is_locked());
#if 0
	vLedSetRed(pll_is_locked());
#endif
}

void pll_init(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_PLL_INHIBIT);
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC->PLL_LOCK);
	pll_inhibit(0);

	pio_irq_register(OPENPICC->PLL_LOCK, &pll_lock_change_cb);
	pio_irq_enable(OPENPICC->PLL_LOCK);
}
