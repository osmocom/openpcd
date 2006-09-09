/* AT91SAM7 ADC controller routines for OpenPCD / OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
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

static void adc_int(void)
{
	u_int32_t sr = adc->ADC_SR;
	struct req_ctx *rctx = adc_state.rctx;
	u_int16_t *val = (u_int16_t *) &(rctx->tx.data[rctx->tx.tot_len]);

	switch (adc_state.state) {
	case ADC_NONE:
		break;
	case ADC_READ_CONTINUOUS_USB:
		if (sr & AT91C_ADC_ENDRX) {
			/* rctx full, get rid of it */
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
			adc_state.state = ADC_NONE;
			adc_state.rctx = NULL;

			/* Disable EOC interrupts since we don't want to
			 * re-start conversion any further*/
			AT91F_ADC_DisableIt(AT91C_BASE_ADC, 
					    AT91C_ADC_EOC4|AT91C_ADC_EOC5);
			AT91F_PDC_DisableRx(AT91C_BASE_PDC_ADC);
		}

		if (sr & (AT91C_ADC_EOC4|AT91C_ADC_EOC5)) {
			/* re-start conversion, since we need more values */
			AT91F_ADC_StartConversion(adc);
		}
		break;
	}
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
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];

	switch (poh->cmd) {
	case OPENPCD_CMD_ADC_READ:
		//channel = poh->reg;
		if (adc_state.rctx) {
			/* FIXME: do something */
			req_ctx_put(rctx);
		}

		adc_state.state = ADC_READ_CONTINUOUS_USB;
		adc_state.rctx = rctx;
		AT91F_ADC_EnableChannel(AT91C_BASE_ADC, OPENPICC_ADC_CH_FIELDSTR);
		AT91F_ADC_EnableIt(AT91C_BASE_ADC, AT91C_ADC_ENDRX |
				   OPENPICC_ADC_CH_FIELDSTR);
		AT91F_PDC_SetRx(AT91C_BASE_PDC_ADC, rctx->rx.data, 60 /*FIXME*/);
		AT91F_PDC_EnableRx(AT91C_BASE_PDC_ADC);
		break;
	}
}

int adc_init(void)
{
	AT91F_ADC_CfgPMC();
	AT91F_ADC_CfgTimings(AT91C_BASE_ADC, 48 /*MHz*/, 5 /*MHz*/,
			     20/*uSec*/, 700/*nSec*/);
	AT91F_ADC_EnableChannel(AT91C_BASE_ADC, OPENPICC_ADC_CH_FIELDSTR |
				OPENPICC_ADC_CH_PLL_DEM);

	usb_hdlr_register(&adc_usb_in, OPENPCD_CMD_CLS_ADC);
}
