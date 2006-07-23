
#include <unistd.h>
#include <stdlib.h>
#include <include/types.h>
#include "openpcd.h"
#include "dbgu.h"

/* FIXME: locking, FIFO order processing */

static struct req_ctx req_ctx[8];
static u_int8_t req_ctx_busy;		/* bitmask of used request contexts */

struct req_ctx *req_ctx_find_get(void)
{
	u_int8_t i;
	for (i = 0; i < NUM_REQ_CTX; i++) {
		if (!(req_ctx_busy & (1 << i))) {
			req_ctx_busy |= (1 << i);
			return &req_ctx[i];
		}
	}

	return NULL;
}

struct req_ctx *req_ctx_find_busy(void)
{
	u_int8_t i;
	for (i = 0; i < NUM_REQ_CTX; i++) {
		if (req_ctx_busy & (1 << i))
			return &req_ctx[i];
	}
}


u_int8_t req_ctx_num(struct req_ctx *ctx)
{
	return ((void *)ctx - (void *)&req_ctx[0])/sizeof(*ctx);
}

void req_ctx_put(struct req_ctx *ctx)
{
	int offset = req_ctx_num(ctx);
	if (offset > NUM_REQ_CTX)
		DEBUGPCR("Error in offset calculation req_ctx_put");

	req_ctx_busy &= ~(1 << offset);
}
