#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <asm/bitops.h>

#include "openpcd.h"
#include "dbgu.h"

/* FIXME: locking, FIFO order processing */

static struct req_ctx req_ctx[NUM_REQ_CTX];

struct req_ctx *req_ctx_find_get(unsigned long old_state, unsigned long new_state)
{
	unsigned long flags;
	u_int8_t i;

	for (i = 0; i < NUM_REQ_CTX; i++) {
		local_irq_save(flags);
		if (req_ctx[i].state == old_state) {
			req_ctx[i].state = new_state;
			local_irq_restore(flags);
			return &req_ctx[i];
		}
		local_irq_restore(flags);
	}

	return NULL;
}

u_int8_t req_ctx_num(struct req_ctx *ctx)
{
	return ((void *)ctx - (void *)&req_ctx[0])/sizeof(*ctx);
}

void req_ctx_set_state(struct req_ctx *ctx, unsigned long new_state)
{
	unsigned long flags;

	/* FIXME: do we need this kind of locking, we're UP! */
	local_irq_save(flags);
	ctx->state = new_state;
	local_irq_restore(flags);
}

void req_ctx_put(struct req_ctx *ctx)
{
	req_ctx_set_state(ctx, RCTX_STATE_FREE);
}
