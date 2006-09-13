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
	return ((char *)ctx - (char *)&req_ctx[0])/sizeof(*ctx);
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
