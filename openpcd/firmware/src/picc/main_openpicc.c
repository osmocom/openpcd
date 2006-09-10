#include <errno.h>
#include <include/lib_AT91SAM7.h>
#include <include/openpcd.h>
#include <os/dbgu.h>
#include "ssc_picc.h"
#include <os/led.h>
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include "../openpcd.h"
#include <os/main.h>
#include <os/pwm.h>
#include <os/tc_cdiv.h>
#include <picc/pll.h>
#include <picc/load_modulation.h>

static const u_int16_t cdivs[] = { 128, 64, 32, 16 };
static int cdiv_idx = 0;

static u_int16_t duty_percent = 22;
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static u_int32_t pwm_freq[] = { 105937, 211875, 423750, 847500 };
static u_int8_t pwm_freq_idx = 0;

static u_int8_t load_mod = 0;

void _init_func(void)
{
	pll_init();
	poti_init();
	load_mod_init();
	tc_cdiv_init();
	pwm_init();
	//adc_init();
	//ssc_rx_init();
	// ssc_tx_init();
}

static void help(void)
{
	DEBUGPCR("q: poti decrease       q: poti increase\r\n"
		 "e: poti retransmit     P: PLL inhibit toggle");
	DEBUGPCR("o: decrease duty       p: increase duty\r\n"
		 "k: stop pwm            l: start pwn\r\n"
		 "n: decrease freq       m: incresae freq");
	DEBUGPCR("u: PA23 const 1        y: PA23 const 0\r\n"
		 "t: PA23 PWM0\r\n"
		 "{: decrease cdiv_idx   }: increse cdiv idx\r\n"
		 "<: decrease cdiv_phase >: increase cdiv_phase");
}

int _main_dbgu(char key)
{
	unsigned char value;
	static u_int8_t poti = 64;
	static u_int8_t pll_inh = 1;

	DEBUGPCRF("main_dbgu");

	switch (key) {
	case 'q':
		if (poti > 0)
			poti--;
		poti_comp_carr(poti);
		DEBUGPCRF("Poti: %u", poti);
		break;
	case 'w':
		if (poti < 127)
			poti++;
		poti_comp_carr(poti);
		DEBUGPCRF("Poti: %u", poti);
		break;
	case 'e':
		poti_comp_carr(poti);
		DEBUGPCRF("Poti: %u", poti);
		break;
	case 'P':
		pll_inh++;
		pll_inh &= 0x01;
		pll_inhibit(pll_inh);
		DEBUGPCRF("PLL Inhibit: %u\n", pll_inh);
		break;
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
		DEBUGPCRF("PA23 output high");
		AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, AT91C_PIO_PA23);
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, AT91C_PIO_PA23);
		break;
	case 'y':
		DEBUGPCRF("PA23 output low");
		AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, AT91C_PIO_PA23);
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, AT91C_PIO_PA23);
		break;
	case 't':
		DEBUGPCRF("PA23 PeriphA (PWM)");
		AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, AT91C_PA23_PWM0);
		break;
	case '?':
		help();
		break;
	case '<':
		tc_cdiv_phase_inc();
		break;
	case '>':
		tc_cdiv_phase_dec();
		break;
	case '{':
		if (cdiv_idx > 0)
			cdiv_idx--;
		tc_cdiv_set_divider(cdivs[cdiv_idx]);
		break;
	case '}':
		if (cdiv_idx < ARRAY_SIZE(cdivs)-1)
			cdiv_idx++;
		tc_cdiv_set_divider(cdivs[cdiv_idx]);
		break;
	case 'v':
		if (load_mod > 0)
			load_mod--;
		load_mod_level(load_mod);
		DEBUGPCR("load_mod: %u\n", load_mod);
		break;
	case 'b':
		if (load_mod < 3)
			load_mod++;
		load_mod_level(load_mod);
		DEBUGPCR("load_mod: %u\n", load_mod);
		break;
	}

	return -EINVAL;
}

void _main_func(void)
{
	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();

	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	usb_in_process();

	//ssc_rx_unthrottle();
}
