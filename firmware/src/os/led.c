/* LED support code for OpenPCD
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

#include <sys/types.h>
#include <errno.h>
#include <lib_AT91SAM7.h>
#include <openpcd.h>
#include "../openpcd.h"
#include <os/usb_handler.h>
#include <os/req_ctx.h>
#include <os/dbgu.h>

static const int ledport[] = {
	[1] =	OPENPCD_PIO_LED1,
	[2] =	OPENPCD_PIO_LED2,
};

static int led2port(int led)
{
	if (led == 1)
		return OPENPCD_PIO_LED1;
	else if (led == 2)
		return OPENPCD_PIO_LED2;
	else
		return 0;
}

void led_switch(int led, int on)
{
	int port;
	
	if (led < 1 || led > 2)
		return;

	port = ledport[led];
	if (on)
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, port);
	else
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, port);
}

int led_get(int led)
{
	int port;

	if (led < 1 || led > 2)
		return -1;

	port = ledport[led];

	return !(AT91F_PIO_GetOutputDataStatus(AT91C_BASE_PIOA) & port);
}

int led_toggle(int led)
{
	int on = led_get(led);
	if (on < 0)
		return on;

	if (on)
		led_switch(led, 0);
	else
		led_switch(led, 1);
	
	return !on;
}

void led_init(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED1);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED2);
	led_switch(1, 0);
	led_switch(2, 0);
}
