/* AT91SAM7 PWM routines for OpenPCD / OpenPICC
 *
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 */

#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include <sys/types.h>
#include <errno.h>
#include <openpcd.h>
#include <os/usb_handler.h>
#include <os/pcd_enumerate.h>
#include <os/req_ctx.h>
#include <os/dbgu.h>
#include "../openpcd.h"

#define Hz
#define	kHz	*1000 Hz
#define MHz	*1000 kHz
#define MCLK	(48 MHz)

#if 1
#define DEBUGPWM DEBUGPCRF
#else
#define DEBUGPWM(x, args...)
#endif

static AT91PS_PWMC pwm = AT91C_BASE_PWMC;

/* find highest bit set. returns bit (32..1) or 0 in case no bit set  */
static int fhs(u_int32_t val)
{
	int i;

	for (i = 32; i > 0; i--) {
		if (val & (1 << (i-1)))
			return i;
	}

	return 0;
}

/* set frequency of PWM signal to freq */
int pwm_freq_set(int channel, u_int32_t freq)
{
	/* in order to get maximum resolution, the pre-scaler must be set to
	 * something like freq << 16.  However, the mimimum pre-scaled frequency
	 * we can get is MCLK (48MHz), the minimum is MCLK/(1024*255) =
	 * 48MHz/261120 = 183Hz */
	u_int32_t overall_div;
	u_int32_t presc_total;
	u_int8_t cpre = 0;
	u_int16_t cprd;

	if (freq > MCLK)
		return -ERANGE;
	
	overall_div = MCLK / freq;
	DEBUGPCRF("mclk=%u, freq=%u, overall_div=%u", MCLK, freq, overall_div);

	if (overall_div > 0x7fff) {
		/* divisor is larger than half the maximum CPRD register, we
		 * have to configure prescalers */
		presc_total = overall_div >> 15;

		/* find highest 2^n fitting in prescaler (highest bit set) */
		cpre = fhs(presc_total);
		if (cpre > 0) {
			/* subtract one, because of fhs semantics */
			cpre--;
		}
		cprd = overall_div / (1 << cpre);
	} else
		cprd = overall_div;
	
	DEBUGPCRF("cpre=%u, cprd=%u", cpre, cprd);
	AT91F_PWMC_CfgChannel(AT91C_BASE_PWMC, channel, 
			      cpre|AT91C_PWMC_CPOL, cprd, 1);

	return 0;
}

void pwm_start(int channel)
{
	AT91F_PWMC_StartChannel(AT91C_BASE_PWMC, (1 << channel));
}

void pwm_stop(int channel)
{
	AT91F_PWMC_StopChannel(AT91C_BASE_PWMC, (1 << channel));
}

void pwm_duty_set_percent(int channel, u_int16_t duty)
{
	u_int32_t tmp = pwm->PWMC_CH[channel].PWMC_CPRDR & 0xffff;
	
	tmp = tmp << 16;	/* extend value by 2^16 */
	tmp = tmp / 100;	/* tmp = 1 % of extended cprd */
	tmp = duty * tmp;	/* tmp = 'duty' % of extended cprd */
	tmp = tmp >> 16;	/* un-extend tmp (divide by 2^16) */

	DEBUGPWM("Writing %u to Update register\n", tmp);
	AT91F_PWMC_UpdateChannel(AT91C_BASE_PWMC, channel, tmp);
}

static int pwm_usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];
	u_int32_t *freq;

	switch (poh->cmd) {
	case OPENPCD_CMD_PWM_ENABLE:
		if (poh->val)
			pwm_start(0);
		else
			pwm_stop(0);
		break;
	case OPENPCD_CMD_PWM_DUTY_SET:
		pwm_duty_set_percent(0, poh->val);
		break;
	case OPENPCD_CMD_PWM_DUTY_GET:
		goto respond;
		break;
	case OPENPCD_CMD_PWM_FREQ_SET:
		if (rctx->rx.tot_len < sizeof(*poh)+4)
			break;
		freq = (void *) poh + sizeof(*poh);
		pwm_freq_set(0, *freq);
		break;
	case OPENPCD_CMD_PWM_FREQ_GET:
		goto respond;
		break;
	default:
		break;
	}

	req_ctx_put(rctx);
	return 0;
respond:
	req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
	udp_refill_ep(2, rctx);
	return 1;
}

void pwm_init(void)
{
	/* IMPORTANT: Disable PA17 (SSC TD) output */
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, AT91C_PIO_PA17);

	/* Set PA23 to Peripheral A (PWM0) */
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, OPENPCD_PIO_MFIN_PWM);

	/* Enable Clock for PWM controller */
	AT91F_PWMC_CfgPMC();

	usb_hdlr_register(&pwm_usb_in, OPENPCD_CMD_CLS_PWM);
}

void pwm_fini(void)
{
	usb_hdlr_unregister(OPENPCD_CMD_CLS_PWM);
	AT91F_PMC_DisablePeriphClock(AT91C_BASE_PMC, (1 << AT91C_ID_PWMC));
}
