#ifndef _PIT_H
#define _PIT_H

#include <sys/types.h>

/* This API (but not the code) is modelled after the Linux API */

struct timer_list {
	struct timer_list *next;
	unsigned long expires;
	void (*function)(void *data);
	void *data;
};

extern unsigned long jiffies;

extern void timer_add(struct timer_list *timer);
extern int timer_del(struct timer_list *timer);

extern void pit_init(void);
extern void pit_mdelay(u_int32_t ms);

#endif
