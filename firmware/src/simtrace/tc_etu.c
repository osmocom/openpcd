/* SimTrace TC (Timer / Clock) support code
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

#include "../openpcd.h"

static AT91PS_TCB tcb;
static AT91PS_TC tcetu = AT91C_BASE_TC0;

static u_int16_t waiting_time = 9600;
static u_int16_t clocks_per_etu = 372;
static u_int16_t wait_events;

static __ramfunc void tc_etu_irq(void)
{
	u_int32_t sr = tcetu->TC_SR;
	static u_int16_t nr_events;

	if (sr & AT91C_TC_ETRGS) {
		/* external trigger, i.e. we have seen a bit on I/O */
		//DEBUGPCR("tE");
		nr_events = 0;
		/* Make sure we don't accept any additional external trigger */
		tcetu->TC_CMR &= ~AT91C_TC_ENETRG;
	}

	if (sr & AT91C_TC_CPCS) {
		/* Compare C event has occurred, i.e. 1 etu expired */
		//DEBUGPCR("tC");
		nr_events++;
		if (nr_events >= wait_events) {
			/* enable external triggers again to catch start bit */
			tcetu->TC_CMR |= AT91C_TC_ENETRG;

			/* disable and re-enable clock to make it stop */
			tcetu->TC_CCR = AT91C_TC_CLKDIS;
			tcetu->TC_CCR = AT91C_TC_CLKEN;

			//DEBUGPCR("%u", nr_events);

			/* Indicate that the waiting time has expired */
			iso7816_wtime_expired();
		}
	}
}

static void recalc_nr_events(void)
{
	wait_events = waiting_time/12;
	/* clocks_per_etu * 12 equals 'sbit + 8 data bits + parity + 2 stop bits */
	tcetu->TC_RC = clocks_per_etu * 12;
}

void tc_etu_set_wtime(u_int16_t wtime)
{
	waiting_time = wtime;
	recalc_nr_events();
}

void tc_etu_set_etu(u_int16_t etu)
{
	clocks_per_etu = etu;
	recalc_nr_events();
}

void tc_etu_init(void)
{
	/* Cfg PA4(TCLK0), PA0(TIOA0), PA1(TIOB0) */
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, 
			    AT91C_PA4_TCLK0 | AT91C_PA0_TIOA0 | AT91C_PA1_TIOB0);

	AT91F_PMC_EnablePeriphClock(AT91C_BASE_PMC, 
				    ((unsigned int) 1 << AT91C_ID_TC0));

	/* Connect TCLK0 to XC0 */
	tcb->TCB_BMR &= ~(AT91C_TCB_TC0XC0S);
	tcb->TCB_BMR |=  AT91C_TCB_TC0XC0S_TCLK0;

	/* Register Interrupt handler */
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_TC0,
			      OPENPCD_IRQ_PRIO_TC_FDT,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &tc_etu_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_TC0);

	/* enable interrupts for Compare-C and External Trigger */
	tcetu->TC_IER = AT91C_TC_CPCS | AT91C_TC_ETRGS;

	tcetu->TC_CMR = AT91C_TC_CLKS_XC0 |	/* XC0 (TCLK0) clock */
		        AT91C_TC_WAVE |		/* Wave Mode */
		        AT91C_TC_ETRGEDG_FALLING |/* Ext trig on falling edge */
		        AT91C_TC_EEVT_TIOB |	/* Ext trigger is TIOB0 */
		        AT91C_TC_ENETRG | 	/* Enable ext. trigger */
		        AT91C_TC_WAVESEL_UP_AUTO |/* Wave mode UP */
		        AT91C_TC_ACPA_SET |	/* Set TIOA0 on A compare */
		        AT91C_TC_ACPC_CLEAR |	/* Clear TIOA0 on C compare */
		        AT91C_TC_ASWTRG_CLEAR;	/* Clear TIOa0 on software trigger */

	tc_etu_set_etu(372);

	/* Enable master clock for TC0 */
	tcetu->TC_CCR = AT91C_TC_CLKEN;

	/* Reset to start timers */
	tcb->TCB_BCR = 1;
}
