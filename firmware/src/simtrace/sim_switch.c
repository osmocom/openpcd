/*
 * (C) 2011 by Harald Welte <hwelte@hmw-consulting.de>
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
#include <sys/types.h>
#include <AT91SAM7.h>
#include <lib_AT91SAM7.h>
#include <openpcd.h>

#include <simtrace_usb.h>

#include <os/usb_handler.h>
#include <os/dbgu.h>
#include <os/pio_irq.h>

#include "../simtrace.h"
#include "../openpcd.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

void sim_switch_mode(int connect_io, int connect_misc)
{
	if (connect_io)
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_IO_SW);
	else
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_IO_SW);

	if (connect_misc)
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SC_SW);
	else
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SC_SW);
}

static void sw_sim_irq(u_int32_t pio)
{

	if (!AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, SIMTRACE_PIO_SW_SIM))
		DEBUGPCR("SIM card inserted");
	else
		DEBUGPCR("SIM card removed");
}

static void vcc_phone_irq(u_int32_t pio)
{
	if (!AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, SIMTRACE_PIO_VCC_PHONE)) {
		DEBUGPCR("VCC_PHONE off");
		/* flush any pending req_ctx to make sure the next ATR
		 * will be aligned to position 0 */
		iso_uart_flush();
	} else
		DEBUGPCR("VCC_PHONE on");
}

void sim_switch_init(void)
{
	DEBUGPCR("ISO_SW Initializing");

	/* make sure we get clock from the power management controller */
	AT91F_US0_CfgPMC();

	/* configure both signals as output */
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SC_SW |
					     SIMTRACE_PIO_IO_SW);

	/* configure sim card detect */
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, SIMTRACE_PIO_SW_SIM);
	AT91F_PIO_CfgInputFilter(AT91C_BASE_PIOA, SIMTRACE_PIO_SW_SIM);
	pio_irq_register(SIMTRACE_PIO_SW_SIM, &sw_sim_irq);
	pio_irq_enable(SIMTRACE_PIO_SW_SIM);
	/* configure VCC_PHONE detection */
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, SIMTRACE_PIO_VCC_PHONE);
	AT91F_PIO_CfgPullupDis(AT91C_BASE_PIOA, SIMTRACE_PIO_VCC_PHONE);
	AT91F_PIO_CfgInputFilter(AT91C_BASE_PIOA, SIMTRACE_PIO_VCC_PHONE);
	pio_irq_register(SIMTRACE_PIO_VCC_PHONE, &vcc_phone_irq);
	pio_irq_enable(SIMTRACE_PIO_VCC_PHONE);

#if 0
	AT91F_ADC_CfgPMC();
	AT91F_ADC_EnableChannel(AT91C_BASE_ADC, AT91C_ADC_CH7);
#endif
}
