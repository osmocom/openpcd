#ifndef _SYSTEM_IRQ_H
#define _SYSTEM_IRQ_H

#include <sys/types.h>

enum sysirqs {
	AT91SAM7_SYSIRQ_PIT 	= 0,
	AT91SAM7_SYSIRQ_DBGU	= 1,
	AT91SAM7_SYSIRQ_EFC	= 2,
	AT91SAM7_SYSIRQ_WDT	= 3,
	AT91SAM7_SYSIRQ_RTT	= 4,
	AT91SAM7_SYSIRQ_RSTC	= 5,
	AT91SAM7_SYSIRQ_PMC	= 6,
	AT91SAM7_SYSIRQ_COUNT
};

typedef void sysirq_hdlr(u_int32_t sr);

extern void sysirq_register(enum sysirqs irq, sysirq_hdlr *hdlr);
extern void sysirq_init(void);

#endif
