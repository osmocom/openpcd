#include <errno.h>
#include <include/lib_AT91SAM7.h>
#include <include/openpcd.h>
#include "dbgu.h"
#include "rc632.h"
#include "led.h"
#include "pcd_enumerate.h"
#include "usb_handler.h"
#include "openpcd.h"
#include "main.h"

void _init_func(void)
{
	rc632_init();
	udp_init();
	rc632_test(RAH);
}

int _main_dbgu(char key)
{
	return -EINVAL;
}

void _main_func(void)
{
	struct req_ctx *rctx;

	/* first we try to get rid of pending to-be-sent stuff */
	while (rctx = req_ctx_find_get(RCTX_STATE_UDP_EP3_PENDING,
				       RCTX_STATE_UDP_EP3_BUSY)) {
		DEBUGPCRF("EP3_BUSY for ctx %u", req_ctx_num(rctx));
		if (udp_refill_ep(3, rctx) < 0)
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP3_PENDING);
	}

	while (rctx = req_ctx_find_get(RCTX_STATE_UDP_EP2_PENDING,
				       RCTX_STATE_UDP_EP2_BUSY)) {
		DEBUGPCRF("EP2_BUSY for ctx %u", req_ctx_num(rctx));
		if (udp_refill_ep(2, rctx) < 0)
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
	}

	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	usb_in_process();

	rc632_unthrottle();
}
