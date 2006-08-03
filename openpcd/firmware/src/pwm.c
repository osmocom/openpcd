/* AT91SAM7 PWM routines for OpenPCD / OpenPICC
 *
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 */

#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include <sys/types.h>
#include <errno.h>
#include "dbgu.h"

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

void pwm_init(void)
{
	/* Set PA0 to Peripheral A (PWM0) */
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, AT91C_PA0_PWM0, 0);

	/* Enable Clock for PWM controller */
	AT91F_PWMC_CfgPMC();
}

void pwm_fini(void)
{
	AT91F_PMC_DisablePeriphClock(AT91C_BASE_PMC, (1 << AT91C_ID_PWMC));
}
