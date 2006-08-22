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

static char usb_buf1[64];
static char usb_buf2[64];
static struct req_ctx dummy_rctx1;


static void help(void)
{
}

void _init_func(void)
{
	DEBUGPCR("\r\n===> main_usb <===\r\n");
	help();

	udp_init();

	memset(usb_buf1, '1', sizeof(usb_buf1));
	memset(usb_buf2, '2', sizeof(usb_buf2));

	dummy_rctx1.tx.tot_len = sizeof(usb_buf1);
	memcpy(dummy_rctx1.tx.data, usb_buf1, sizeof(usb_buf1));

}

int _main_dbgu(char key)
{
	switch (key) {
	default:
		return -EINVAL;
	}

	return 0;
}

void _main_func(void)
{
	/* first we try to get rid of pending to-be-sent stuff */
	//usb_out_process();

	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	//usb_in_process();

	/* try unthrottling sources since we now are [more] likely to
	 * have empty request contexts */
	//udp_unthrottle();

	while (udp_refill_ep(2, &dummy_rctx1) < 0) ;

	DEBUGP("S");

	//led_toggle(2);
}
