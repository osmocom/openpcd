/* SAM7S system interrupt demultiplexer
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

#undef DEBUG

#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>

#include <os/system_irq.h>
#include <os/dbgu.h>
#include <string.h>

#include "../openpcd.h"

static sysirq_hdlr *sysirq_hdlrs[AT91SAM7_SYSIRQ_COUNT];

void sys_irq(uint32_t previous_pc)
{
	uint32_t sr;

	/* Somehow Atmel decided to do really stupid interrupt sharing
	 * for commonly-used interrupts such as the timer irq */

	/* dbgu */
	if (*AT91C_DBGU_IMR) {
		sr = *AT91C_DBGU_CSR;
		if (sr & *AT91C_DBGU_IMR) {
			DEBUGP("DBGU(");
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_DBGU]) {
				DEBUGP("handler ");
				sysirq_hdlrs[AT91SAM7_SYSIRQ_DBGU](sr);
			} else {
				*AT91C_DBGU_IDR = *AT91C_DBGU_IMR;
				DEBUGP("no handler ");
			}
			DEBUGP(") ");
		}
	}
	
	/* pit_irq */
	if (*AT91C_PITC_PIMR & AT91C_PITC_PITIEN) {
		sr = *AT91C_PITC_PISR;
		if (sr & AT91C_PITC_PITS) {
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_PIT]) {
				sysirq_hdlrs[AT91SAM7_SYSIRQ_PIT](sr);
			} else {
				DEBUGP("no handler DISABLE_PIT ");
				*AT91C_PITC_PIMR &= ~AT91C_PITC_PITIEN;
			}
		}
	}

	/* rtt_irq */
	if (*AT91C_RTTC_RTMR & (AT91C_RTTC_ALMIEN|AT91C_RTTC_RTTINCIEN)) {
		sr = *AT91C_RTTC_RTSR;
		if (sr) {
			DEBUGP("RTT(");
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_RTT]) {
				DEBUGP("handler ");
				sysirq_hdlrs[AT91SAM7_SYSIRQ_RTT](sr);
			} else {
				*AT91C_RTTC_RTMR &= ~(AT91C_RTTC_ALMIEN|
						      AT91C_RTTC_RTTINCIEN);
				DEBUGP("no handler ");
			}
			DEBUGP(") ");
		}
	}

	/* pmc_irq */
	if (*AT91C_PMC_IMR) {
		sr = *AT91C_PMC_SR;
		if (sr & *AT91C_PMC_IMR) {
			DEBUGP("PMC(");
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_PMC]) {
				DEBUGP("handler ");
				sysirq_hdlrs[AT91SAM7_SYSIRQ_PMC](sr);
			} else {
				*AT91C_PMC_IDR = *AT91C_PMC_IMR;
				DEBUGP("no handler ");
			}
			DEBUGP(") ");
		}
	}

	/* rstc_irq */
	if (*AT91C_RSTC_RMR & (AT91C_RSTC_URSTIEN|AT91C_RSTC_BODIEN)) {
		sr = *AT91C_RSTC_RSR;
		if (sr & (AT91C_RSTC_URSTS|AT91C_RSTC_BODSTS)) {
			DEBUGP("RSTC(");
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_RSTC]) {
				DEBUGP("handler ");
				sysirq_hdlrs[AT91SAM7_SYSIRQ_RSTC](sr);
			} else {
				*AT91C_RSTC_RMR &= ~(AT91C_RSTC_URSTIEN|
						     AT91C_RSTC_BODIEN);
				DEBUGP("no handler ");
			}
			DEBUGP(") ");
		}
	}

	/* mc_irq */
	if (*AT91C_MC_FMR & (AT91C_MC_LOCKE | AT91C_MC_PROGE)) {
		sr = *AT91C_MC_FSR;
		if ((*AT91C_MC_FMR & AT91C_MC_LOCKE && (sr & AT91C_MC_LOCKE))||
		    (*AT91C_MC_FMR & AT91C_MC_PROGE && (sr & AT91C_MC_PROGE))){
			DEBUGP("EFC(");
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_EFC]) {
				DEBUGP("handler ");
		    		sysirq_hdlrs[AT91SAM7_SYSIRQ_EFC](sr);
			} else {
				*AT91C_MC_FMR &= ~(AT91C_MC_LOCKE |
						   AT91C_MC_PROGE);
				DEBUGP("no handler ");
			}
			DEBUGP(") ");
		}
	}
	    
	/* wdt_irq */
	if (*AT91C_WDTC_WDMR & AT91C_WDTC_WDFIEN) {
		sr = *AT91C_WDTC_WDSR;
		if (sr) {
			char dbg_buf[100];
			sprintf(dbg_buf, "sys_irq [Old PC = %08X]\n\r", previous_pc);
			AT91F_DBGU_Frame(dbg_buf);

			DEBUGP("WDT(");
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_WDT]) {
				DEBUGP("handler ");
				sysirq_hdlrs[AT91SAM7_SYSIRQ_WDT](sr);
			} else {
				/* we can't disable it... */
				DEBUGP("no handler ");
			}
			DEBUGP(") ");
		}
	}
	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_SYS);
	DEBUGPCR("END");
}

static void sysirq_entry(void)
{
	/* DON'T MODIFY THIS SECTION AND Cstartup.S/IRQ_Handler_Entry */
	register unsigned *previous_pc asm("r0");
	asm("ADD R1, SP, #16; LDR R0, [R1]");
	sys_irq(previous_pc);
}

void sysirq_register(enum sysirqs irq, sysirq_hdlr *hdlr)
{
	if (irq >= AT91SAM7_SYSIRQ_COUNT)
		return;

	sysirq_hdlrs[irq] = hdlr;
}

void sysirq_init(void)
{
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SYS,
			      OPENPCD_IRQ_PRIO_SYS,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL,
			      &sysirq_entry);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SYS);
}
