#ifndef _PIO_IRQ_H
#define _PIO_IRQ_H

#include "openpicc.h"

#define NR_PIO 32
typedef void irq_handler_t(u_int32_t pio);

extern void pio_irq_enable(u_int32_t pio);
extern void pio_irq_disable(u_int32_t pio);
extern int pio_irq_register(u_int32_t pio, irq_handler_t *func);
extern void pio_irq_unregister(u_int32_t pio);
extern void pio_irq_init(void);

#endif
