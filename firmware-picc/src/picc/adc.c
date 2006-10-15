/* AT91SAM7 ADC controller routines for OpenPICC
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
#include <sys/types.h>
#include <AT91SAM7.h>
#include <lib_AT91SAM7.h>
#include <openpcd.h>

#include <os/usb_handler.h>
#include "../openpcd.h"
#include <os/dbgu.h>

#define OPENPICC_ADC_CH_FIELDSTR	AT91C_ADC_CH4
#define OPENPICC_ADC_CH_PLL_DEM		AT91C_ADC_CH5

#define DEBUG_ADC

#ifdef DEBUG_ADC
#define DEBUGADC	DEBUGP
#else
#define DEBUGADC	do { } while (0)
#endif

static const AT91PS_ADC adc = AT91C_BASE_ADC;

enum adc_states {
	ADC_NONE,
	ADC_READ_CONTINUOUS,
	ADC_READ_CONTINUOUS_USB,
	ADC_READ_SINGLE,
};

struct adc_state {
	enum adc_states state;
	struct req_ctx *rctx;
};

static struct adc_state adc_state;

static void adc_irq(void)
{
	u_int32_t sr = adc->ADC_SR;
	struct req_ctx *rctx = adc_state.rctx;

	DEBUGADC("adc_irq(SR=0x%08x, IMR=0x%08x, state=%u): ",
		 sr, adc->ADC_IMR, adc_state.state);

	switch (adc_state.state) {
	case ADC_NONE:
		//break;
	case ADC_READ_CONTINUOUS_USB:
		if (sr & AT91C_ADC_EOC4)
			DEBUGADC("CDR4=0x%4x ", adc->ADC_CDR4);
		if (sr & AT91C_ADC_EOC5)
			DEBUGADC("CDR5=0x%4x ", adc->ADC_CDR5);
		if (sr & AT91C_ADC_ENDRX) {
			/* rctx full, get rid of it */
			DEBUGADC("sending rctx (val=%s) ",
				 hexdump(rctx->data[4], 2));
				 
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
			adc_state.state = ADC_NONE;
			adc_state.rctx = NULL;
		
			//AT91F_PDC_SetRx(AT91C_BASE_PDC_ADC, NULL, 0);

			/* Disable EOC interrupts since we don't want to
			 * re-start conversion any further*/
			AT91F_ADC_DisableIt(AT91C_BASE_ADC, AT91C_ADC_ENDRX);
					    //AT91C_ADC_EOC4|AT91C_ADC_EOC5|AT91C_ADC_ENDRX);
			AT91F_PDC_DisableRx(AT91C_BASE_PDC_ADC);
			DEBUGADC("disabled IT/RX ");
		} else {
			if (sr & (AT91C_ADC_EOC4|AT91C_ADC_EOC5)) {
				/* re-start conversion, since we need more values */
				AT91F_ADC_StartConversion(adc);
			}
		}
		break;
	}

	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_ADC);
	DEBUGADC("cleeared ADC IRQ in AIC\r\n");
}

#if 0
u_int16_t adc_read_fieldstr(void)
{
	return adc->ADC_CDR4;
}

u_int16_T adc_read_pll_dem(void)
{
	return adc
}
#endif

static int adc_usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->data[0];

	switch (poh->cmd) {
	case OPENPCD_CMD_ADC_READ:
		DEBUGADC("ADC_READ(chan=%u, len=%u) ", poh->reg, poh->val);
		//channel = poh->reg;
		if (adc_state.rctx) {
			/* FIXME: do something */
			req_ctx_put(rctx);
		}

		adc_state.state = ADC_READ_CONTINUOUS_USB;
		adc_state.rctx = rctx;
		rctx->tot_len = sizeof(*poh) + poh->val * 2;
		AT91F_PDC_SetRx(AT91C_BASE_PDC_ADC, rctx->data, poh->val);
		AT91F_PDC_EnableRx(AT91C_BASE_PDC_ADC);
		AT91F_ADC_EnableChannel(AT91C_BASE_ADC, OPENPICC_ADC_CH_FIELDSTR);
		AT91F_ADC_EnableIt(AT91C_BASE_ADC, AT91C_ADC_ENDRX |
				   OPENPICC_ADC_CH_FIELDSTR);
		AT91F_ADC_StartConversion(adc);
		break;
	}
}

int adc_init(void)
{
	AT91F_ADC_CfgPMC();
	AT91F_ADC_CfgTimings(AT91C_BASE_ADC, 48 /*MHz*/, 5 /*MHz*/,
			     20/*uSec*/, 700/*nSec*/);
#if 0
	AT91F_ADC_EnableChannel(AT91C_BASE_ADC, OPENPICC_ADC_CH_FIELDSTR |
				OPENPICC_ADC_CH_PLL_DEM);
#endif
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_ADC,
			      AT91C_AIC_PRIOR_LOWEST,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &adc_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_ADC);

	usb_hdlr_register(&adc_usb_in, OPENPCD_CMD_CLS_ADC);
}
