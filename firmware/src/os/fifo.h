#ifndef _FIFO_H
#define _FIFO_H

#include <sys/types.h>

#define FIFO_SIZE	1024

struct fifo {
	u_int16_t size;		/* actual FIFO size, can be smaller than 'data' */
	u_int16_t producer;	/* index of producer */
	u_int16_t consumer;	/* index of consumer */
	u_int16_t watermark;
	u_int8_t irq;
	u_int8_t irq_en;
	u_int8_t status;
	void (*callback)(struct fifo *fifo, u_int8_t event, void *data);
	void *cb_data;
	u_int8_t data[FIFO_SIZE];
};


extern int fifo_init(struct fifo *fifo, u_int16_t size, 
		     void (*callback)(struct fifo *fifo, u_int8_t event, void *data), void *cb_data);
extern u_int16_t fifo_data_get(struct fifo *fifo, u_int16_t len, u_int8_t *data);
extern u_int16_t fifo_data_put(struct fifo *fifo, u_int16_t len, u_int8_t *data);
extern int fifo_available(struct fifo *fifo);

#endif
