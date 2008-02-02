/* OpenPC TC (Timer / Clock) support code
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
 * This idea of this code is to feed the 13.56MHz carrier clock of RC632
 * into TCLK1, which is routed to XC1.  Then configure TC0 to divide this
 * clock by a configurable divider.
 *
 */

#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include "dbgu.h"

#include "openpicc.h"
#include "tc_cdiv.h"
#include "led.h"

AT91PS_TCB tcb = AT91C_BASE_TCB;

/* set carrier divider to a specific */
void __ramfunc tc_cdiv_set_divider(u_int16_t div)
{
	if(tcb->TCB_TC0.TC_CV > div
#ifdef OPENPICC_USE_CLOCK_GATING
	/* Don't spin if FRAME_BURST is clear, the clock is stopped in this case */
	&& !(!AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_PIO_FRAME_BURST))
#endif
	) {
		while(tcb->TCB_TC0.TC_CV > 3); /* Three carrier cycles are about 10 clock cycles, should be enough for the loop */  
	}
	tcb->TCB_TC0.TC_RC = div;

	/* set to 50% duty cycle */
	tcb->TCB_TC0.TC_RA = 1;
	tcb->TCB_TC0.TC_RB = 1 + (div >> 1);
	
	/* We must reset CV to zero when it was greater than RC.
	 * In order to not lose phase information when doing that we'll busy wait till CV is
	 * zero modulo the new RC.*/
	/*tc_cdiv_phase_add(tcb->TCB_TC0.TC_RC-(tcb->TCB_TC0.TC_CV%tcb->TCB_TC0.TC_RC));*/
	if(tcb->TCB_TC0.TC_CV > div
#ifdef OPENPICC_USE_CLOCK_GATING
	/* Don't spin if FRAME_BURST is clear, the clock is stopped in this case */
	&& !(!AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_PIO_FRAME_BURST))
#endif
	) {
		while(tcb->TCB_TC0.TC_CV % div != 0);
		tcb->TCB_TC0.TC_CCR = AT91C_TC_SWTRG;
	}
}

void __ramfunc tc_cdiv_phase_add(int16_t inc)
{
	tcb->TCB_TC0.TC_RA = (tcb->TCB_TC0.TC_RA + inc) % tcb->TCB_TC0.TC_RC;
	tcb->TCB_TC0.TC_RB = (tcb->TCB_TC0.TC_RB + inc) % tcb->TCB_TC0.TC_RC;

	/* FIXME: can this be done more elegantly? */
	if (tcb->TCB_TC0.TC_RA == 0) {
		tcb->TCB_TC0.TC_RA += 1;
		tcb->TCB_TC0.TC_RB += 1;
	}
}

void tc_cdiv_reset(void)
{
	/* Reset to start timers */
	tcb->TCB_BCR = 1;
}

void tc_cdiv_init(void)
{
	/* Cfg PA28(TCLK1), PA0(TIOA0), PA1(TIOB0), PA20(TCLK2) as Periph B */
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, 
			    OPENPICC_PIO_CARRIER_IN |
			    OPENPICC_PIO_CARRIER_DIV_OUT |
			    OPENPICC_PIO_CDIV_HELP_OUT |
			    OPENPICC_PIO_CDIV_HELP_IN
#ifdef OPENPICC_USE_CLOCK_GATING
				| OPENPICC_PIO_FRAME_BURST
#endif
			    );

	AT91F_PMC_EnablePeriphClock(AT91C_BASE_PMC, 
				    ((unsigned int) 1 << AT91C_ID_TC0));

	/* Enable Clock for TC0 */
	tcb->TCB_TC0.TC_CCR = AT91C_TC_CLKEN;

	/* Connect TCLK1 to XC1, TCLK2 to XC2 */
	tcb->TCB_BMR &= ~(AT91C_TCB_TC1XC1S | AT91C_TCB_TC2XC2S);
	tcb->TCB_BMR |=  (AT91C_TCB_TC1XC1S_TCLK1 | AT91C_TCB_TC2XC2S_TCLK2);
#ifdef OPENPICC_USE_CLOCK_GATING
	/* Connect TCLK0 to XC0 */
	tcb->TCB_BMR &= ~(AT91C_TCB_TC0XC0S);
	tcb->TCB_BMR |=  (AT91C_TCB_TC0XC0S_TCLK0);
#endif

	/* Clock XC1, Wave mode, Reset on RC comp
	 * TIOA0 on RA comp = set, * TIOA0 on RC comp = clear,
	 * TIOB0 on EEVT = set, TIOB0 on RB comp = clear,
	 * EEVT = XC2 (TIOA0) 
	 * if OPENPICC_MODIFIED_BOARD: BURST on XC0 */
	tcb->TCB_TC0.TC_CMR = AT91C_TC_CLKS_XC1 | AT91C_TC_WAVE |
			      AT91C_TC_WAVESEL_UP_AUTO | 
			      AT91C_TC_ACPA_SET | AT91C_TC_ACPC_CLEAR |
			      AT91C_TC_BEEVT_SET | AT91C_TC_BCPB_CLEAR |
			      AT91C_TC_EEVT_XC2 | AT91C_TC_ETRGEDG_RISING |
			      AT91C_TC_BSWTRG_CLEAR | AT91C_TC_ASWTRG_CLEAR
#ifdef OPENPICC_USE_CLOCK_GATING
				| AT91C_TC_BURST_XC0
#endif
			      ;

	tc_cdiv_set_divider(128);

	tc_cdiv_reset();
}

void tc_cdiv_print(void)
{
	DEBUGP("TCB_BMR=0x%08x ", tcb->TCB_BMR);
	DEBUGP("TC0_CV=0x%08x ", tcb->TCB_TC0.TC_CV);
	DEBUGP("TC0_CMR=0x%08x ", tcb->TCB_TC0.TC_CMR);
	DEBUGPCR("TC0_SR=0x%08x", tcb->TCB_TC0.TC_SR);

	DEBUGPCR("TC0_RA=0x%04x, TC0_RB=0x%04x, TC0_RC=0x%04x",
		 tcb->TCB_TC0.TC_RA, tcb->TCB_TC0.TC_RB, tcb->TCB_TC0.TC_RC);
}

void tc_cdiv_fini(void)
{
	tcb->TCB_TC0.TC_CCR = AT91C_TC_CLKDIS;
	AT91F_PMC_DisablePeriphClock(AT91C_BASE_PMC,
				     ((unsigned int) 1 << AT91C_ID_TC0));
}
