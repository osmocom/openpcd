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
#include <os/dbgu.h>
#include <os/system_irq.h>

#include "../openpcd.h"

/* PIT runs at MCK/16 (= 3MHz) */
#define PIV_MS(x)		(x * 3000)

static struct timer_list *timers;

volatile unsigned long jiffies;

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

static int __timer_remove(struct timer_list *old)
{
	struct timer_list *tl, *tl_prev = NULL;

	for (tl = timers; tl != NULL; tl = tl->next) {
		if (tl == old) {
			if (tl == timers)
				timers = tl->next;
			else
				tl_prev->next = tl->next;
			return 1;
		}
		tl_prev = tl;
	}

	return 0;
}

int timer_del(struct timer_list *tl)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);
	ret = __timer_remove(tl);
	local_irq_restore(flags);

	return ret;
}

void timer_add(struct timer_list *tl)
{
	unsigned long flags;

	local_irq_save(flags);
	__timer_insert(tl);
	local_irq_restore(flags);
}

static void pit_irq(uint32_t sr)
{
	struct timer_list *tl, *next;

	if (!(sr & 0x1))
		return;

	jiffies += *AT91C_PITC_PIVR >> 20;

	/* this is the most simple/stupid algorithm one can come up with, but
	 * hey, we're on a small embedded platform with only a hand ful
	 * of timers, at all */
	for (tl = timers; tl != NULL; tl = next) {
		next = tl->next;
		if (tl->expires <= jiffies) {
			/* delete timer from list */
			timer_del(tl);
			tl->function(tl->data);
		}
	}
}

void pit_mdelay(uint32_t ms)
{
	uint32_t end;

	end = (AT91F_PITGetPIIR(AT91C_BASE_PITC) + ms) % 20;

	while (end < AT91F_PITGetPIIR(AT91C_BASE_PITC)) { }
}

void mdelay(uint32_t ms)
{
	return pit_mdelay(ms);
}

void usleep(uint32_t us)
{
	return;
	return pit_mdelay(us/1000);
}

void pit_init(void)
{
	AT91F_PITC_CfgPMC();

	AT91F_PITInit(AT91C_BASE_PITC, 1000000/HZ /* uS */, 48 /* MHz */);

	sysirq_register(AT91SAM7_SYSIRQ_PIT, &pit_irq);	

	AT91F_PITEnableInt(AT91C_BASE_PITC);
}
