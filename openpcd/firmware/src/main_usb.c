/* main_usb - OpenPCD test firmware for benchmarking USB performance
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
 */

#include <errno.h>
#include <string.h>
#include <lib_AT91SAM7.h>
#include "openpcd.h"
#include "rc632.h"
#include "dbgu.h"
#include "led.h"
#include "pwm.h"
#include "tc.h"
#include "ssc.h"
#include "pcd_enumerate.h"
#include "usb_handler.h"

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
	usbtest_init();
}

void _main_func(void)
{
	/* first we try to get rid of pending to-be-sent stuff */
	//usb_out_process();

	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	usb_in_process();

	/* try unthrottling sources since we now are [more] likely to
	 * have empty request contexts */
	udp_unthrottle();
}
