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
#include "../simtrace.h"
#include <os/main.h>
#include <os/pio_irq.h>

#include <simtrace/tc_etu.h>
#include <simtrace/iso7816_uart.h>
#include <simtrace/sim_switch.h>
#include <simtrace_usb.h>

enum simtrace_md {
	SIMTRACE_MD_OFF,
	SIMTRACE_MD_SNIFFER,
	SIMTRACE_MD_MITM,
};

#define UART1_PINS (SIMTRACE_PIO_nRST_PH |		\
		    SIMTRACE_PIO_CLK_PH |		\
		    SIMTRACE_PIO_CLK_PH_T |		\
		    SIMTRACE_PIO_IO_PH_RX |		\
		    SIMTRACE_PIO_IO_PH_TX)

#define UART0_PINS (SIMTRACE_PIO_nRST |			\
		    SIMTRACE_PIO_CLK |			\
		    SIMTRACE_PIO_CLK_T |		\
		    SIMTRACE_PIO_IO |			\
		    SIMTRACE_PIO_IO_T)

enum simtrace_power_mode {
	SIMTRACE_PWR_OFF,
	SIMTRACE_PWR_PASS,
	SIMTRACE_PWR_LDO
};

void simtrace_set_power(enum simtrace_power_mode mode)
{
	switch (mode) {
	case SIMTRACE_PWR_PASS:
		/* switch VCC_SIM pin into output mode, as in the first
		 * generation prototype we use it directly to supply Vcc
		 * to the SIM.  Pin unused in v1.0(p) and v1.1p */
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_VCC_SIM);
		/* v1.0/1.1: pass-throguh power from the reader/phone (FPC) */
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SIM_LDOEN);
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SIM_nPWRFWD);
		break;
	case SIMTRACE_PWR_LDO:
		/* switch VCC_SIM pin into output mode, as in the first
		 * generation prototype we use it directly to supply Vcc
		 * to the SIM.  Pin unused in v1.0(p) and v1.1p */
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_VCC_SIM);
		/* v1.0/1.1: power from the internal 3.3V LDO */
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SIM_nPWRFWD);
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SIM_LDOEN);
		break;
	default:
	case SIMTRACE_PWR_OFF:
		/* switch VCC_SIM pin into output mode, as in the first
		 * generation prototype we use it directly to supply Vcc
		 * to the SIM.  Pin unused in v1.0(p) and v1.1p */
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_VCC_SIM);
		/* switch all power paths off */
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SIM_LDOEN);
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SIM_nPWRFWD);
		break;
	}
}

static void simtrace_set_mode(enum simtrace_md mode)
{
	switch (mode) {
	case SIMTRACE_MD_SNIFFER:
		DEBUGPCR("MODE: SNIFFER\n");

		/* switch UART1 pins to input, no pull-up */
		AT91F_PIO_CfgInput(AT91C_BASE_PIOA, UART1_PINS);
		AT91F_PIO_CfgPullupDis(AT91C_BASE_PIOA, UART1_PINS);

		/* switch UART0 pins to 'ISO7816 card mode' */
		AT91F_PIO_CfgInput(AT91C_BASE_PIOA, UART0_PINS);
		AT91F_PIO_CfgPullupDis(AT91C_BASE_PIOA, UART0_PINS);
		AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, SIMTRACE_PIO_IO, SIMTRACE_PIO_CLK);
		sim_switch_mode(1, 1);
		break;
	case SIMTRACE_MD_MITM:
		DEBUGPCR("MODE: MITM\n");
		/* switch UART1 pins to 'ISO7816 card mode' */
		/* switch UART0 pins to 'ISO7816 reader mode' */
		sim_switch_mode(0, 0);
		break;
	}
}

static int simtrace_usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->data[0];
	struct simtrace_stats *stats_in;
	struct simtrace_stats *stats = (struct simtrace_stats *) poh->data;

	switch (poh->cmd) {
	case SIMTRACE_MSGT_STATS:
		stats_in = iso_uart_stats_get();
		memcpy(stats, stats_in, sizeof(*stats));
		req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
		break;
	default:
		req_ctx_set_state(rctx, RCTX_STATE_FREE);
		break;
	}
}

void _init_func(void)
{
	/* low-level hardware initialization */
	pio_irq_init();
	iso_uart_init();
	tc_etu_init();
	sim_switch_init();

	usbtest_init();
	usb_hdlr_register(&simtrace_usb_in, OPENPCD_CMD_CLS_ADC);

	/* high-level protocol */
	//opicc_usbapi_init();
	led_switch(1, 0);
	led_switch(2, 1);

	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_VCC_SIM);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SIM_nPWRFWD);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_SIM_LDOEN);
	/* default: use local LDO to supply power */
	simtrace_set_power(SIMTRACE_PWR_LDO);

	iso_uart_rx_mode();
	simtrace_set_mode(SIMTRACE_MD_SNIFFER);
}


static void help(void)
{
	DEBUGPCR("r: iso uart Rx mode\r\n"
		 "c: toggle clock master/slave\r\n"
		 "l: set nRST to low (active)\r\n"
		 "h: set nRST to high (inactive)\r\n"
		 "o: set nRST to input\r\n"
		 "t: ISO UART statistics\r\n"
		 "s: disconnect SIM bus switch\r\n"
		 "S: connect SIM bus switch\r\n");
}

int _main_dbgu(char key)
{
	static int i = 0;
	DEBUGPCRF("main_dbgu");

	switch (key) {
	case 's':
		simtrace_set_mode(SIMTRACE_MD_MITM);
		break;
	case 'S':
		simtrace_set_mode(SIMTRACE_MD_SNIFFER);
		break;
	case 't':
		iso_uart_stats_dump();
		break;
	case 'r':
		iso_uart_rx_mode();
		break;
	case 'c':
		iso_uart_clk_master(i++ & 1);
		break;
	case 'l':
		iso_uart_rst(0);
		break;
	case 'h':
		iso_uart_rst(1);
		break;
	case 'o':
		iso_uart_rst(2);
		break;
	case 'd':
		iso_uart_dump();
		break;
	case '?':
		help();
		break;
	}

	return -EINVAL;
}

void _main_func(void)
{
	static unsigned loopLow = 0, loopHigh = 0;

	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();

	/* next we deal with incoming requests from USB EP1 (OUT) */
	usb_in_process();

	udp_unthrottle();

	if ((loopLow & 0xFFFF) == 0) {
		DEBUGPCR("Heart beat %08X", loopHigh++);
	}
	loopLow++;

	iso_uart_report_overrun();
}
