/* AT91SAM7 Watch Dog Timer code for OpenPCD / OpenPICC
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

#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>

#include <os/dbgu.h>
#include <os/system_irq.h>

#ifdef  WDT_DEBUG
#undef  WDT_DEBUG
#endif/*WDT_DEBUG*/
 
static void wdt_irq(u_int32_t sr)
{
	DEBUGPCRF("================> WATCHDOG EXPIRED !!!!!");
}

void wdt_restart(void)
{
	AT91F_WDTRestart(AT91C_BASE_WDTC);
}

void wdt_init(void)
{
	sysirq_register(AT91SAM7_SYSIRQ_WDT, &wdt_irq);
#ifdef WDT_DEBUG
	AT91F_WDTSetMode(AT91C_BASE_WDTC, (0xff << 16) |
			 AT91C_WDTC_WDDBGHLT | AT91C_WDTC_WDIDLEHLT |
			 AT91C_WDTC_WDFIEN);
#else
	AT91F_WDTSetMode(AT91C_BASE_WDTC, (0x80 << 16) |
			 AT91C_WDTC_WDRSTEN | 0x80);
#endif
}
