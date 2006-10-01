/* Periodic Interval Timer Implementation for OpenPCD
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
 *
 * TODO: handle jiffies wrap correctly!
 */

#include <errno.h>
#include <sys/types.h>
#include <asm/system.h>

#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include <os/pit.h>

#include "../openpcd.h"

/* PIT runs at MCK/16 (= 3MHz) */
#define PIV_MS(x)		(x * 3000)

static struct timer_list *timers;

unsigned long jiffies;

static void __timer_insert(struct timer_list *new)
{
	struct timer_list *tl, *tl_prev = NULL;

	if (!timers) {
		/* optimize for the common, fast case */
		new->next = NULL;
		timers = new;
		return;
	}

	for (tl = timers; tl != NULL; tl = tl->next) {
		if (tl->expires < new->expires) {
			/* we need ot add just before this one */
			if (!tl_prev) {
				new->next = timers;
				timers = new;
			} else {
				new->next = tl;
				tl_prev->next = new;
			}
		}
		tl_prev = tl;
	}
}

static void __timer_remove(struct timer_list *old)
{
	struct timer_list *tl, *tl_prev = NULL;

	for (tl = timers; tl != NULL; tl = tl->next) {
		if (tl == old) {
			if (tl == timers)
				timers = tl->next;
			else
				tl_prev->next = tl->next;
		}
		tl_prev = tl;
	}
}

int timer_del(struct timer_list *tl)
{
	unsigned long flags;

	local_irq_save(flags);
	__timer_remove(tl);
	local_irq_restore(flags);
}

void timer_add(struct timer_list *tl)
{
	unsigned long flags;

	local_irq_save(flags);
	__timer_insert(tl);
	local_irq_restore(flags);
}

static void pit_irq(void)
{
	struct timer_list *tl;
	unsigned long flags;

	jiffies++;

	/* this is the most simple/stupid algorithm one can come up with, but
	 * hey, we're on a small embedded platform with only a hand ful
	 * of timers, at all */
	for (tl = timers; tl != 0; tl = tl->next) {
		if (tl->expires >= jiffies) {
			tl->function(tl->data);
			local_irq_save(flags);
			/* delete timer from list */
			__timer_remove(tl);
			local_irq_restore(flags);
		}
	}
}

void pit_mdelay(u_int32_t ms)
{
	u_int32_t end;

	end = (AT91F_PITGetPIIR(AT91C_BASE_PITC) + ms) % 20;

	while (end < AT91F_PITGetPIIR(AT91C_BASE_PITC)) { }
}

void pit_init(void)
{
	AT91F_PITC_CfgPMC();

	AT91F_PITInit(AT91C_BASE_PITC, 1000 /* uS */, 48 /* MHz */);

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SYS,
			      OPENPCD_IRQ_PRIO_PIT,
			      AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE,
			      &pit_irq);

	//AT91F_PITEnableInt(AT91C_BASE_PITC);
}
