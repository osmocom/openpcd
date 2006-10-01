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

#include "../openpcd.h"

static sysirq_hdlr *sysirq_hdlrs[AT91SAM7_SYSIRQ_COUNT];

static void sys_irq(void)
{
	u_int32_t sr;
	DEBUGP("sys_irq: ");

	/* Somehow Atmel decided to do really stupid interrupt sharing
	 * for commonly-used interrupts such as the timer irq */

	/* dbgu */
	if (*AT91C_DBGU_IMR) {
		sr = *AT91C_DBGU_CSR;
		DEBUGP("DBGU(");
		if (sr & *AT91C_DBGU_IMR) {
			DEBUGP("found ");
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_DBGU]) {
				DEBUGP("handler ");
				sysirq_hdlrs[AT91SAM7_SYSIRQ_DBGU](sr);
			} else {
				*AT91C_DBGU_IDR = *AT91C_DBGU_IMR;
				DEBUGP("no handler ");
			}
		}
		DEBUGP(") ");
	}
	
	/* pit_irq */
	if (*AT91C_PITC_PIMR & AT91C_PITC_PITIEN) {
		sr = *AT91C_PITC_PISR;
		DEBUGP("PIT(");
		if (sr & AT91C_PITC_PITS) {
			DEBUGP("found ");
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_PIT]) {
				DEBUGP("handler ");
				sysirq_hdlrs[AT91SAM7_SYSIRQ_PIT](sr);
			} else {
				*AT91C_PITC_PIMR &= ~AT91C_PITC_PITIEN;
				DEBUGP("no handler ");
			}
		}
		DEBUGP(") ");
	}

	/* rtt_irq */
	if (*AT91C_RTTC_RTMR & (AT91C_RTTC_ALMIEN|AT91C_RTTC_RTTINCIEN)) {
		sr = *AT91C_RTTC_RTSR;
		DEBUGP("RTT(");
		if (sr) {
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_RTT])
				sysirq_hdlrs[AT91SAM7_SYSIRQ_RTT](sr);
			else
				*AT91C_RTTC_RTMR &= ~(AT91C_RTTC_ALMIEN|
						      AT91C_RTTC_RTTINCIEN);
		}
		DEBUGP(") ");
	}

	/* pmc_irq */
	if (*AT91C_PMC_IMR) {
		sr = *AT91C_PMC_SR;
		DEBUGP("PMC(");
		if (sr & *AT91C_PMC_IMR) {
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_PMC])
				sysirq_hdlrs[AT91SAM7_SYSIRQ_PMC](sr);
			else
				*AT91C_PMC_IDR = *AT91C_PMC_IMR;
		}
		DEBUGP(") ");
	}

	/* rstc_irq */
	if (*AT91C_RSTC_RMR & (AT91C_RSTC_URSTIEN|AT91C_RSTC_BODIEN)) {
		sr = *AT91C_RSTC_RSR;
		DEBUGP("RSTC(");
		if (sr & (AT91C_RSTC_URSTS|AT91C_RSTC_BODSTS)) {
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_RSTC])
				sysirq_hdlrs[AT91SAM7_SYSIRQ_RSTC](sr);
			else
				*AT91C_RSTC_RMR &= ~(AT91C_RSTC_URSTIEN|
						     AT91C_RSTC_BODIEN);
		}
		DEBUGP(") ");
	}

	/* mc_irq */
	if (*AT91C_MC_FMR & (AT91C_MC_LOCKE | AT91C_MC_PROGE)) {
		sr = *AT91C_MC_FSR;
		DEBUGP("EFC(");
		if ((*AT91C_MC_FMR & AT91C_MC_LOCKE && (sr & AT91C_MC_LOCKE))||
		    (*AT91C_MC_FMR & AT91C_MC_PROGE && (sr & AT91C_MC_PROGE))){
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_EFC])
		    		sysirq_hdlrs[AT91SAM7_SYSIRQ_EFC](sr);
			else
				*AT91C_MC_FMR &= ~(AT91C_MC_LOCKE |
						   AT91C_MC_PROGE);
		}
		DEBUGP(") ");
	}
	    
	/* wdt_irq */
	if (*AT91C_WDTC_WDMR & AT91C_WDTC_WDFIEN) {
		sr = *AT91C_WDTC_WDSR;
		DEBUGP("WDT(");
		if (sr) {
			if (sysirq_hdlrs[AT91SAM7_SYSIRQ_WDT])
				sysirq_hdlrs[AT91SAM7_SYSIRQ_WDT](sr);
			/* we can't disable it... */
		}
		DEBUGP(") ");
	}
	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_SYS);
	DEBUGPCR("");
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
			      AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE,
			      &sys_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SYS);
}
