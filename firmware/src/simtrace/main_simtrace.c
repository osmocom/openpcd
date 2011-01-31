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
#include <os/pio_irq.h>

#include <simtrace/tc_etu.h>
#include <simtrace/iso7816_uart.h>

void _init_func(void)
{
	/* low-level hardware initialization */
	pio_irq_init();
	iso_uart_init();
	tc_etu_init();

	usbtest_init();

	/* high-level protocol */
	//opicc_usbapi_init();
	led_switch(1, 0);
	led_switch(2, 1);

	iso_uart_rx_mode();
}

static void help(void)
{
	DEBUGPCR("r: iso uart Rx mode\r\n"
		 "c: toggle clock master/slave\r\n"
		 "l: set nRST to low (active)\r\n"
		 "h: set nRST to high (inactive)\r\n"
		 "o: set nRST to input\r\n");
}

int _main_dbgu(char key)
{
	static int i = 0;
	DEBUGPCRF("main_dbgu");

	switch (key) {
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
	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();

	/* next we deal with incoming requests from USB EP1 (OUT) */
	usb_in_process();

	udp_unthrottle();
}
