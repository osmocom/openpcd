/* OpenPICC TC (Timer / Clock) support code
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


/* PICC Simulator Side:
 * In order to support responding to synchronous frames (REQA/WUPA/ANTICOL),
 * we need a second Timer/Counter (TC2).  This unit is reset by an external
 * event (rising edge of modulation pause PCD->PICC) connected to TIOB2, and
 * counts up to a configurable number of carrier clock cycles (RA). Once the
 * RA value is reached, TIOA2 will see a rising edge.  This rising edge will
 * be interconnected to TF (Tx Frame) of the SSC to start transmitting our
 * synchronous response.
 *
 */

#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include <os/dbgu.h>

#include "../openpcd.h"
#include <os/tc_cdiv.h>
#include <picc/tc_fdt.h>

void tc_fdt_set(u_int16_t count)
{
	tcb->TCB_TC2.TC_RA = count;
}

void tc_fdt_init(void)
{
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, AT91C_PA15_TF,
			    AT91C_PA26_TIOA2 | AT91C_PA27_TIOB2);
	AT91F_PMC_EnablePeriphClock(AT91C_BASE_PMC,
				    ((unsigned int) 1 << AT91C_ID_TC2));
	/* Enable Clock for TC2 */
	tcb->TCB_TC2.TC_CCR = AT91C_TC_CLKEN;

	/* Clock XC1, Wave Mode, No automatic reset on RC comp
	 * TIOA2 in RA comp = set, TIOA2 on RC comp = clear,
	 * TIOB2 as input, EEVT = TIOB2, Reset/Trigger on EEVT */
	tcb->TCB_TC2.TC_CMR = AT91C_TC_CLKS_XC1 | AT91C_TC_WAVE |
			      AT91C_TC_WAVESEL_UP |
			      AT91C_TC_ACPA_SET | AT91C_TC_ACPC_CLEAR |
			      AT91C_TC_BEEVT_NONE | AT91C_TC_BCPB_NONE |
			      AT91C_TC_EEVT_TIOB | AT91C_TC_ETRGEDG_RISING |
			      AT91C_TC_ENETRG ;

	/* Reset to start timers */
	tcb->TCB_BCR = 1;
}

