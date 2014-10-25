/* AT91SAM7 USB Request Context for OpenPCD / OpenPICC
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

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <asm/bitops.h>
#include <os/dbgu.h>
#include <os/req_ctx.h>

#include "../openpcd.h"

/* FIXME: locking, FIFO order processing */

#if defined(__AT91SAM7S64__) || defined(RUN_FROM_RAM)
#define NUM_RCTX_SMALL 16
#define NUM_RCTX_LARGE 1
#else
#define NUM_RCTX_SMALL 8
#define NUM_RCTX_LARGE 4
#endif

#define NUM_REQ_CTX	(NUM_RCTX_SMALL+NUM_RCTX_LARGE)

static u_int8_t rctx_data[NUM_RCTX_SMALL][RCTX_SIZE_SMALL];
static u_int8_t rctx_data_large[NUM_RCTX_LARGE][RCTX_SIZE_LARGE];

static struct req_ctx req_ctx[NUM_REQ_CTX];

/* queue of RCTX indexed by their current state */
static struct req_ctx *req_ctx_queues[RCTX_STATE_COUNT], *req_ctx_tails[RCTX_STATE_COUNT];

struct req_ctx __ramfunc *req_ctx_find_get(int large,
				 unsigned long old_state, 
				 unsigned long new_state)
{
	struct req_ctx *toReturn;
	unsigned long flags;

	if (old_state >= RCTX_STATE_COUNT || new_state >= RCTX_STATE_COUNT) {
		DEBUGPCR("Invalid parameters for req_ctx_find_get");
		return NULL;
	}
	local_irq_save(flags);
	toReturn = req_ctx_queues[old_state];
	if (toReturn) {
		if ((req_ctx_queues[old_state] = toReturn->next))
			toReturn->next->prev = NULL;
		else
			req_ctx_tails[old_state] = NULL;
		if ((toReturn->prev = req_ctx_tails[new_state]))
			toReturn->prev->next = toReturn;
		else
			req_ctx_queues[new_state] = toReturn;
		req_ctx_tails[new_state] = toReturn;
		toReturn->state = new_state;
		toReturn->next = NULL;
	}
	local_irq_restore(flags);
	return toReturn;
}

u_int8_t req_ctx_num(struct req_ctx *ctx)
{
	return ctx - req_ctx;
}

void req_ctx_set_state(struct req_ctx *ctx, unsigned long new_state)
{
	unsigned long flags;
	unsigned old_state;

	if (new_state >= RCTX_STATE_COUNT) {
		DEBUGPCR("Invalid new_state for req_ctx_set_state");
		return;
	}
	local_irq_save(flags);
	old_state = ctx->state;
	if (ctx->prev)
		ctx->prev->next = ctx->next;
	else
		req_ctx_queues[old_state] = ctx->next;
	if (ctx->next)
		ctx->next->prev = ctx->prev;
	else
		req_ctx_tails[old_state] = ctx->prev;

	if ((ctx->prev = req_ctx_tails[new_state]))
		ctx->prev->next = ctx;
	else
		req_ctx_queues[new_state] = ctx;
	req_ctx_tails[new_state] = ctx;
	ctx->state = new_state;
	ctx->next = NULL;
	local_irq_restore(flags);
}

#ifdef DEBUG_REQCTX
void req_print(int state) {
	int count = 0;
	struct req_ctx *ctx, *last = NULL;
	DEBUGP("State [%02i] start <==> ", state);
	ctx = req_ctx_queues[state];
	while (ctx) {
		if (last != ctx->prev)
			DEBUGP("*INV_PREV* ");
		DEBUGP("%08X => ", ctx);
		last = ctx;
		ctx = ctx->next;
		count++;
		if (count > NUM_REQ_CTX) {
		  DEBUGP("*WILD POINTER* => ");
		  break;
		}
	}
	DEBUGPCR("NULL");
	if (!req_ctx_queues[state] && req_ctx_tails[state]) {
		DEBUGPCR("NULL head, NON-NULL tail");
	}
	if (last != req_ctx_tails[state]) {
		DEBUGPCR("Tail does not match last element");
	}
}
#endif

void req_ctx_put(struct req_ctx *ctx)
{
	unsigned long intcFlags;
	unsigned old_state;

	local_irq_save(intcFlags);
	old_state = ctx->state;
	if (ctx->prev)
		ctx->prev->next = ctx->next;
	else
		req_ctx_queues[old_state] = ctx->next;
	if (ctx->next)
		ctx->next->prev = ctx->prev;
	else
		req_ctx_tails[old_state] = ctx->prev;

	if ((ctx->prev = req_ctx_tails[RCTX_STATE_FREE]))
		ctx->prev->next = ctx;
	else
		req_ctx_queues[RCTX_STATE_FREE] = ctx;
	req_ctx_tails[RCTX_STATE_FREE] = ctx;
	ctx->state = RCTX_STATE_FREE;
	ctx->next = NULL;
	local_irq_restore(intcFlags);
}

void req_ctx_init(void)
{
	int i;
	for (i = 0; i < NUM_RCTX_SMALL; i++) {
		req_ctx[i].prev = req_ctx + i - 1;
		req_ctx[i].next = req_ctx + i + 1;
		req_ctx[i].size = RCTX_SIZE_SMALL;
		req_ctx[i].tot_len = 0;
		req_ctx[i].data = rctx_data[i];
		req_ctx[i].state = RCTX_STATE_FREE;
		DEBUGPCR("SMALL req_ctx[%02i] initialized at %08X, Data: %08X => %08X",
			i, req_ctx + i, req_ctx[i].data, req_ctx[i].data + RCTX_SIZE_SMALL);
	}

	for (; i < NUM_REQ_CTX; i++) {
		req_ctx[i].prev = req_ctx + i - 1;
		req_ctx[i].next = req_ctx + i + 1;
		req_ctx[i].size = RCTX_SIZE_LARGE;
		req_ctx[i].tot_len = 0;
		req_ctx[i].data = rctx_data_large[i];
		req_ctx[i].state = RCTX_STATE_FREE;
		DEBUGPCR("LARGE req_ctx[%02i] initialized at %08X, Data: %08X => %08X",
			i, req_ctx + i, req_ctx[i].data, req_ctx[i].data + RCTX_SIZE_LARGE);
	}
	req_ctx[0].prev = NULL;
	req_ctx[NUM_REQ_CTX - 1].next = NULL;

	req_ctx_queues[RCTX_STATE_FREE] = req_ctx;
	req_ctx_tails[RCTX_STATE_FREE] = req_ctx + NUM_REQ_CTX - 1;

	for (i = RCTX_STATE_FREE + 1; i < RCTX_STATE_COUNT; i++) {
		req_ctx_queues[i] = req_ctx_tails[i] = NULL;
	}
}
