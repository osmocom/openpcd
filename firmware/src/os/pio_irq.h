#ifndef _PIO_IRQ_H
#define _PIO_IRQ_H

#define NR_PIO 32
typedef void irq_handler_t(uint32_t pio);

extern void pio_irq_enable(uint32_t pio);
extern void pio_irq_disable(uint32_t pio);
extern int pio_irq_register(uint32_t pio, irq_handler_t *func);
extern void pio_irq_unregister(uint32_t pio);
extern void pio_irq_init(void);

#endif
