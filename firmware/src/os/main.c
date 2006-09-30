/* USB Device Firmware for OpenPCD
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

#include <errno.h>
#include <string.h>
#include <compile.h>
#include <include/lib_AT91SAM7.h>
#include <os/dbgu.h>
#include <os/led.h>
#include <os/main.h>
#include <os/power.h>
#include <os/usbcmd_generic.h>
#include <os/pcd_enumerate.h>
#include "../openpcd.h"

#include <compile.h>

const struct openpcd_compile_version opcd_version = {
	.svnrev = COMPILE_SVNREV,
	.date = COMPILE_DATE,
	.by = COMPILE_BY,
};

int main(void)
{
	/* initialize LED and debug unit */
	led_init();
	AT91F_DBGU_Init();

	AT91F_PIOA_CfgPMC();

	wdt_init();

	/* initialize USB */
	req_ctx_init();
	usbcmd_gen_init();
	udp_open();

	/* call application specific init function */
	_init_func();

	// Enable User Reset and set its minimal assertion to 960 us
	AT91C_BASE_RSTC->RSTC_RMR =
	    AT91C_RSTC_URSTEN | (0x4 << 8) | (unsigned int)(0xA5 << 24);

#ifdef DEBUG_CLOCK_PA6
	AT91F_PMC_EnablePCK(AT91C_BASE_PMC, 0, AT91C_PMC_CSS_PLL_CLK);
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, AT91C_PA6_PCK0);
#endif

	/* switch on first led */
	led_switch(2, 1);

	DEBUGPCRF("entering main (idle) loop");
	while (1) {
		/* Call application specific main idle function */
		_main_func();
		dbgu_rb_flush();
		wdt_restart();
#ifdef CONFIG_IDLE
		//cpu_idle();
#endif
	}
}
