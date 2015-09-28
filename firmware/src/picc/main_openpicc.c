/* OpenPICC Main Program 
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
#include <include/lib_AT91SAM7.h>
#include <include/openpcd.h>
#include <os/dbgu.h>
#include <os/led.h>
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include "../openpcd.h"
#include <os/main.h>
#include <os/pwm.h>
#include <os/tc_cdiv.h>
#include <os/pio_irq.h>
#include <picc/da.h>
#include <picc/pll.h>
#include <picc/ssc_picc.h>
#include <picc/load_modulation.h>

static const uint16_t cdivs[] = { 8192, 2048, 1024, 512, 128, 64, 32, 16 };
static uint8_t cdiv_idx = 6;

static uint16_t duty_percent = 22;
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static uint32_t pwm_freq[] = { 105937, 211875, 423750, 847500 };
static uint8_t pwm_freq_idx = 0;

static uint8_t load_mod = 0;

#define DA_BASELINE 192

void _init_func(void)
{
	/* low-level hardware initialization */
	pio_irq_init();
	pll_init();
	da_init();
	load_mod_init();
	tc_cdiv_init();
	//tc_fdt_init();
	pwm_init();
	adc_init();
	ssc_rx_init();
	ssc_tx_init();

	/* high-level protocol */
	decoder_init();
	opicc_usbapi_init();

	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_PIO_BOOTLDR);
	da_comp_carr(DA_BASELINE);
}

static void help(void)
{
	DEBUGPCR("q: da decrease       w: da increase\r\n"
		 "e: da retransmit     P: PLL inhibit toggle");
	DEBUGPCR("o: decrease duty       p: increase duty\r\n"
		 "k: stop pwm            l: start pwn\r\n"
		 "n: decrease freq       m: incresae freq");
	DEBUGPCR("u: PA23 const 1        y: PA23 const 0\r\n"
		 "t: PA23 PWM0           L: display PLL LOCK\r\n"
		 "{: decrease cdiv_idx   }: increse cdiv idx\r\n"
		 "<: decrease cdiv_phase >: increase cdiv_phase");
	DEBUGPCR("v: decrease load_mod   b: increase load_mod\r\n"
		 "B: read button         S: toggle nSLAVE_RESET\r\n"
		 "a: SSC stop            s: SSC start\r\n"
		 "d: SSC mode select     T: TC_CDIV_HELP enable\r\n");
}

int _main_dbgu(char key)
{
	static uint8_t poti = DA_BASELINE;
	static uint8_t pll_inh = 1;
	static uint8_t ssc_mode = 1;
	static uint8_t sync_enabled = 0;

	DEBUGPCRF("main_dbgu");

	switch (key) {
	case 'q':
		if (poti > 0)
			poti--;
		da_comp_carr(poti);
		DEBUGPCRF("DA: %u", poti);
		break;
	case 'w':
		if (poti < 255)
			poti++;
		da_comp_carr(poti);
		DEBUGPCRF("DA: %u", poti);
		break;
	case 'e':
		da_comp_carr(poti);
		DEBUGPCRF("DA: %u", poti);
		break;
	case 'P':
		pll_inh++;
		pll_inh &= 0x01;
		pll_inhibit(pll_inh);
		DEBUGPCRF("PLL Inhibit: %u", pll_inh);
		break;
	case 'L':
		DEBUGPCRF("PLL Lock: %u", pll_is_locked());
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
	case 'B':
		DEBUGPCRF("Button status: %u\n", 
			  AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, AT91F_PIO_IsInputSet));
		break;
	case 'S':
		if (AT91F_PIO_IsOutputSet(AT91C_BASE_PIOA, OPENPICC_PIO_nSLAVE_RESET)) {
			AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_nSLAVE_RESET);
			DEBUGPCRF("nSLAVE_RESET == LOW");
		} else {
			AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_nSLAVE_RESET);
			DEBUGPCRF("nSLAVE_RESET == HIGH");
		}
		break;
	case 'a':
		DEBUGPCRF("SSC RX STOP");
		ssc_rx_stop();
		break;
	case 's':
		DEBUGPCRF("SSC RX START");
		ssc_rx_start();
		break;
	case 'd':
		ssc_mode++;
		if (ssc_mode >= 6)
			ssc_mode = 0;
		ssc_rx_mode_set(ssc_mode);
		DEBUGPCRF("SSC MODE %u", ssc_mode);
		break;
	case 'T':
		if (sync_enabled) {
			tc_cdiv_sync_disable();
			sync_enabled = 0;
		} else  {
			tc_cdiv_sync_enable();
			sync_enabled = 1;
		}
		break;
	}

	tc_cdiv_print();
	//tc_fdt_print();
	ssc_print();

	return -EINVAL;
}

void _main_func(void)
{
	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();

	/* next we deal with incoming requests from USB EP1 (OUT) */
	usb_in_process();

	udp_unthrottle();
	ssc_rx_unthrottle();
}
