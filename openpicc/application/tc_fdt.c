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
 * event (rising edge of modulation pause PCD->PICC, falling edge of
 * demodulated data) connected to TIOB2, and counts up to a configurable
 * number of carrier clock cycles (RA). Once the RA value is reached, TIOA2
 * will see a rising edge.  This rising edge will be interconnected to TF (Tx
 * Frame) of the SSC to start transmitting our synchronous response.
 *
 */

#include <FreeRTOS.h>
#include <task.h>
#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include "dbgu.h"

#include "openpicc.h"
#include "led.h"
#include "tc_cdiv.h"
#include "tc_fdt.h"
#include "usb_print.h"
#include "ssc_picc.h"
#include "cmd.h"

static AT91PS_TC tcfdt = AT91C_BASE_TC2;

void tc_fdt_set(u_int16_t count)
{
	tcfdt->TC_RA = count;
}

void __ramfunc tc_fdt_set_to_next_slot(int last_bit)
{
	int reference_time;
	if(last_bit == 0) reference_time = ISO14443A_FDT_OFFSET_0;
	else reference_time = ISO14443A_FDT_OFFSET_1;
	
	if(tcfdt->TC_SR & AT91C_TC_CLKSTA) 
		while(tcfdt->TC_CV != 0xFFFF && (tcfdt->TC_CV - reference_time) % 128 != 0);
	tcfdt->TC_CCR = AT91C_TC_SWTRG;
	tc_fdt_set(2*128);
}


/* 'count' number of carrier cycles after the last modulation pause, 
 * we deem the frame to have ended */
void tc_frame_end_set(u_int16_t count)
{
	tcfdt->TC_RB = count;
}

static void __ramfunc tc_fdt_irq(void) __attribute__ ((naked));
static void __ramfunc tc_fdt_irq(void)
{
	portSAVE_CONTEXT();
	//vLedSetGreen(1);
	u_int32_t sr = tcfdt->TC_SR;
	DEBUGP("tc_fdt_irq: TC2_SR=0x%08x TC2_CV=0x%08x ", 
		sr, tcfdt->TC_CV);

	if (sr & AT91C_TC_ETRGS) {
		DEBUGP("Ext_trigger ");
	}
	if (sr & AT91C_TC_CPAS) {
		DEBUGP("FDT_expired ");
	}
	if (sr & AT91C_TC_CPBS) {
	usb_print_string_f("tc_cpbs ", 0);
		DEBUGP("Frame_end ");
		ssc_rx_stop_frame_ended();
	}
	if (sr & AT91C_TC_CPCS) {
		DEBUGP("Compare_C ");
	}
	DEBUGPCR("");
	AT91F_AIC_AcknowledgeIt();
	//vLedSetGreen(0);
	portRESTORE_CONTEXT();
}

void tc_fdt_print(void)
{
	DEBUGP("TC2_CV=0x%08x ", tcfdt->TC_CV);
	DEBUGP("TC2_CMR=0x%08x ", tcfdt->TC_CMR);
	DEBUGP("TC2_SR=0x%08x ", tcfdt->TC_SR);
	DEBUGP("TC2_RA=0x%04x, TC2_RB=0x%04x, TC2_RC=0x%04x",
		tcfdt->TC_RA, tcfdt->TC_RB, tcfdt->TC_RC);
}

void tc_fdt_init(void)
{
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, AT91C_PA15_TF,
			    AT91C_PA26_TIOA2 | AT91C_PA27_TIOB2);
	AT91F_PMC_EnablePeriphClock(AT91C_BASE_PMC,
				    ((unsigned int) 1 << AT91C_ID_TC2));
	/* Enable Clock for TC2 */
	tcfdt->TC_CCR = AT91C_TC_CLKEN;

	tcfdt->TC_RC = 0xffff;
	tc_frame_end_set(128*2);

	/* Clock XC1, Wave Mode, No automatic reset on RC comp
	 * TIOA2 in RA comp = set, TIOA2 on RC comp = clear,
	 * TIOA2 on EEVT = clear, TIOA2 on SWTRG = clear,
	 * TIOB2 as input, EEVT = TIOB2, Reset/Trigger on EEVT */
	tcfdt->TC_CMR = AT91C_TC_CLKS_XC1 | AT91C_TC_WAVE |
		      AT91C_TC_WAVESEL_UP |
		      AT91C_TC_ACPA_SET | AT91C_TC_ACPC_CLEAR |
		      AT91C_TC_AEEVT_CLEAR | AT91C_TC_ASWTRG_CLEAR |
		      AT91C_TC_BEEVT_NONE | AT91C_TC_BCPB_NONE |
		      AT91C_TC_EEVT_TIOB | AT91C_TC_ETRGEDG_FALLING |
		      AT91C_TC_ENETRG | AT91C_TC_CPCSTOP ;

	/* Reset to start timers */
	tcb->TCB_BCR = 1;

	AT91F_AIC_ConfigureIt(AT91C_ID_TC2,
			      OPENPCD_IRQ_PRIO_TC_FDT,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, (THandler)&tc_fdt_irq);
	AT91F_AIC_EnableIt(AT91C_ID_TC2);

	tcfdt->TC_IER = AT91C_TC_CPAS | AT91C_TC_CPBS | AT91C_TC_CPCS | 
			AT91C_TC_ETRGS;
}
