#ifndef _PIT_H
#define _PIT_H

#include <sys/types.h>

#define HZ	100

/* This API (but not the code) is modelled after the Linux API */

struct timer_list {
	struct timer_list *next;
	unsigned long expires;
	void (*function)(void *data);
	void *data;
};

extern volatile unsigned long jiffies;

extern void timer_add(struct timer_list *timer);
extern int timer_del(struct timer_list *timer);

extern void pit_init(void);
extern void pit_mdelay(uint32_t ms);

#endif
