#ifndef _FIFO_H
#define _FIFO_H

#include <sys/types.h>

#define FIFO_SIZE	1024

struct fifo {
	uint16_t size;		/* actual FIFO size, can be smaller than 'data' */
	uint16_t producer;	/* index of producer */
	uint16_t consumer;	/* index of consumer */
	uint16_t watermark;
	uint8_t irq;
	uint8_t irq_en;
	uint8_t status;
	void (*callback)(struct fifo *fifo, uint8_t event, void *data);
	void *cb_data;
	uint8_t data[FIFO_SIZE];
};


extern int fifo_init(struct fifo *fifo, uint16_t size, 
		     void (*callback)(struct fifo *fifo, uint8_t event, void *data), void *cb_data);
extern uint16_t fifo_data_get(struct fifo *fifo, uint16_t len, uint8_t *data);
extern uint16_t fifo_data_put(struct fifo *fifo, uint16_t len, uint8_t *data);
extern int fifo_available(struct fifo *fifo);

#endif
