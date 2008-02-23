/* main_hid - OpenPCD Human Input Device firmware
 *
 * (C) 2008 by Harald Welte <laforge@hmw-consulting.de>
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
 */

#include <errno.h>
#include <string.h>
#include <lib_AT91SAM7.h>
#include <openpcd.h>
#include <os/dbgu.h>
#include <os/led.h>
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>

static void help(void)
{
}

int _main_dbgu(char key)
{
	switch (key) {
	default:
		return -EINVAL;
	}

	return 0;
}

void _init_func(void)
{
	//usbtest_init();
}

void _main_func(void)
{
	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();

	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	usb_in_process();

	/* try unthrottling sources since we now are [more] likely to
	 * have empty request contexts */
	udp_unthrottle();
}
