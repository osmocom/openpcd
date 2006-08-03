/* main_pwm - OpenPCD firmware for generating a PWM-modulated 13.56MHz
 *	      carrier
 *
 * To use this, you need to connect PIOA P0 with MFIN of the reader.
 */

#include <errno.h>
#include <string.h>
#include <lib_AT91SAM7.h>
#include "rc632.h"
#include "dbgu.h"
#include "led.h"
#include "pwm.h"

static u_int8_t force_100ask = 0;
static u_int8_t mod_conductance = 0x3f;
static u_int8_t cw_conductance = 0x3f;
static u_int16_t duty_percent = 22;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static u_int32_t pwm_freq[] = { 105937, 211875, 423750, 847500 };
static u_int8_t pwm_freq_idx = 0;

static void rc632_modulate_mfin()
{
	rc632_reg_write(RAH, RC632_REG_TX_CONTROL, 
			RC632_TXCTRL_MOD_SRC_MFIN|RC632_TXCTRL_TX2_INV|
			RC632_TXCTRL_TX1_RF_EN|RC632_TXCTRL_TX2_RF_EN);
}

/* (77/40) ^ EXPcsCfgCW */
/* 1, 1.925, 3.705625, 7.13332812 */

#define COND_MANT(x)	(x & 0x0f)
#define COND_EXP(x)	((x & 0x30) >> 4)

static u_int16_t rsrel_expfact[] = { 1000, 1925, 3706, 7133 };

static u_int32_t calc_conduct_rel(u_int8_t inp)
{
	u_int32_t cond_rel;

	cond_rel = COND_MANT(inp) * rsrel_expfact[COND_EXP(inp)];
	cond_rel = cond_rel / 1000;

	return cond_rel;
}

static u_int8_t rsrel_table[] = {
	0, 16, 32, 48, 1, 17, 2, 3, 33, 18, 4, 5, 19, 6, 7, 49, 34, 20,
	8, 9, 21, 10, 11, 35, 22, 12, 13, 23, 14, 50, 36, 15, 24, 25,
	37, 26, 27, 51, 38, 28, 29, 39, 30, 52, 31, 40, 41, 53, 42, 43,
	54, 44, 45, 55, 46, 47, 56, 57, 58, 59, 60, 61, 62, 63 };

static void help(void)
{
	DEBUGPCR("o: decrease duty     p: increase duty\r\n"
		 "k: stop pwm          l: start pwn\r\n"
		 "n: decrease freq     m: incresae freq\r\n"
		 "v: decrease mod_cond b: increase mod_cond\r\n"
		 "g: decrease cw_cond  h: increase cw_cond\r\n");
	DEBUGPCR("u: PA0 const 1       y: PA0 const 0\r\n"
		 "t: PA0 PWM0	        f: toggle Force100ASK");
}

void _init_func(void)
{
	DEBUGPCR("\r\n===> main_pwm <===\r\n");
	help();

	rc632_init();
	DEBUGPCRF("turning on RF");
	rc632_turn_on_rf(RAH);

	/* switch PA17 (connected to MFIN on board) to input */
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, AT91C_PIO_PA17);

	pwm_init();
	rc632_modulate_mfin();

	pwm_freq_set(0, 105937);
	pwm_start(0);

	pwm_duty_set_percent(0, 22);	/* 22% of 9.43uS = 2.07uS */
}

int _main_dbgu(char key)
{
	switch (key) {
	case 'o':
		if (duty_percent >= 1)
			duty_percent--;
		pwm_duty_set_percent(0, duty_percent);
		break;
	case 'p':
		if (duty_percent <= 99)
			duty_percent++;
		pwm_duty_set_percent(0, duty_percent);
		break;
	case 'k':
		pwm_stop(0);
		break;
	case 'l':
		pwm_start(0);
		break;
	case 'n':
		if (pwm_freq_idx > 0) {
			pwm_freq_idx--;
			pwm_stop(0);
			pwm_freq_set(0, pwm_freq[pwm_freq_idx]);
			pwm_start(0);
			pwm_duty_set_percent(0, 22);	/* 22% of 9.43uS = 2.07uS */
		}
		break;
	case 'm':
		if (pwm_freq_idx < ARRAY_SIZE(pwm_freq)-1) {
			pwm_freq_idx++;
			pwm_stop(0);
			pwm_freq_set(0, pwm_freq[pwm_freq_idx]);
			pwm_start(0);
			pwm_duty_set_percent(0, 22);	/* 22% of 9.43uS = 2.07uS */
		}
		break;
	case 'u':
		DEBUGPCRF("PA0 output high");
		AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, AT91C_PIO_PA0);
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, AT91C_PIO_PA0);
		break;
	case 'y':
		DEBUGPCRF("PA0 output low");
		AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, AT91C_PIO_PA0);
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, AT91C_PIO_PA0);
		return 0;
		break;
	case 't':
		DEBUGPCRF("PA0 PeriphA (PWM)");
		AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, AT91C_PA0_PWM0, 0);
		return 0;
		break;
	case 'f':
		DEBUGPCRF("%sabling Force100ASK", force_100ask ? "Dis":"En");
		if (force_100ask) {
			force_100ask = 0;
			rc632_clear_bits(RAH, RC632_REG_TX_CONTROL,
				         RC632_TXCTRL_FORCE_100_ASK);
		} else {
			force_100ask = 1;
			rc632_set_bits(RAH, RC632_REG_TX_CONTROL,
				       RC632_TXCTRL_FORCE_100_ASK);
		}
		return 0;
		break;
	case 'v':
		if (mod_conductance > 0) {
			mod_conductance--;
			rc632_reg_write(RAH, RC632_REG_MOD_CONDUCTANCE,
					rsrel_table[mod_conductance]);
		}
		break;
	case 'b':
		if (mod_conductance < 0x3f) {
			mod_conductance++;
			rc632_reg_write(RAH, RC632_REG_MOD_CONDUCTANCE,
					rsrel_table[mod_conductance]);
		}
		break;
	case 'g':
		if (cw_conductance > 0) {
			cw_conductance--;
			rc632_reg_write(RAH, RC632_REG_CW_CONDUCTANCE, 
					rsrel_table[cw_conductance]);
		}
		break;
	case 'h':
		if (cw_conductance < 0x3f) {
			cw_conductance++;
			rc632_reg_write(RAH, RC632_REG_CW_CONDUCTANCE, 
					rsrel_table[cw_conductance]);
		}
		break;
	case '?':
		help();
		return 0;
		break;
	default:
		return -EINVAL;
	}

	DEBUGPCRF("pwm_freq=%u, duty_percent=%u, mod_conductance=%u, cw_conductance=%u", 
		  pwm_freq[pwm_freq_idx], duty_percent, mod_conductance, cw_conductance);
	
	return 0;
}

void _main_func(void)
{
	led_toggle(2);
}
