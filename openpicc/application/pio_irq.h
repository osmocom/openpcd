#ifndef _PIO_IRQ_H
#define _PIO_IRQ_H

#include "openpicc.h"
#include "FreeRTOS.h"

#define NR_PIO 32
typedef portBASE_TYPE irq_handler_t(u_int32_t pio, portBASE_TYPE xTaskWoken);

extern void pio_irq_enable(u_int32_t pio);
extern void pio_irq_disable(u_int32_t pio);
extern long pio_irq_get_count(void);
extern int pio_irq_register(u_int32_t pio, irq_handler_t *func);
extern void pio_irq_unregister(u_int32_t pio);
extern void pio_irq_init(void);
extern void pio_irq_init_once(void);

#endif
