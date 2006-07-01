
#define FIFO_SIZE	1024

/* virtual FIFO */

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

#define FIFO_IRQ_LO	0x01
#define FIFO_IRQ_HI	0x02
#define FIFO_IRQ_OFLOW	0x04

/* returns number of data bytes present in the fifo */
int fifo_available(struct fifo *fifo)
{
	if (fifo->producer > fifo->consumer)
		return fifo->producer - fifo->consumer;
	else
		return (fifo->size - fifo->consumer) + fifo->producer;
}

void fifo_check_water(struct fifo *fifo)
{
	int avail = fifo_available(fifo);

	if (avail <= fifo->watermark)
		irq |= FIFO_IRQ_LO;
	else
		irq &= FIFO_IRQ_LO;

	if (fifo->size - avail >= fifo->watermark)
		irq |= FIFO_IRQ_HI;
	else
		irq &= FIFO_IRQ_HI;
}

void fifo_check_raise_int(struct fifo *fifo)
{
	if (fifo->irq & fifo->irq_en)
		fifo->cb(fifo, fifo->irq, fifo->cb_data);
}


u_int16_t fifo_data_put(struct fifo *fifo, u_int16_t len, u_int8_t *data)
{
	u_int16_t old_producer = fifo->producer;

	if (len > fifo_available(fifo)) {
		len = fifo_available(fifo)
		fifo->irq |= FIFO_IRQ_OFLOW;
	}

	if (len + fifo->producer <= fifo->size) {
		/* easy case */
		memcpy(fifo->data[fifo->producer], data, len);
		fifo->producer += len;
	} else {
		/* difficult: wrap around */
		u_int16_t chunk_len;

		chunk_len = fifo->size - fifo->producer;
		memcpy(fifo->data[fifo->producer], data, chunk_len);

		memcpy(fifo->data[0], data + chunk_len, len - chunk_len);
		fifo->producer = len - chunk_len;
	}

	fifo_check_water(fifo);

	return len;
}


u_int16_t fifo_data_get(struct fifo *fifo, u_int16_t len, u_int8_t *data)
{
	u_int16_t avail = fifo_available(fifo);

	if (avail < len)
		len = avail;

	if (fifo->producer > fifo->consumer) {
		/* easy case */
		memcpy(data, fifo->data[fifo->consumer], len);
	} else {
		/* difficult case: wrap */
		u_int16_t chunk_len = fifo->size - fifo->consumer;
		memcpy(data, fifo->data[fifo->consumer], chunk_len);
		memcpy(data+chunk_len, fifo->data[0], len - chunk_len);
	}

	fifo_check_water(fifo);

	return len;
}

int fifo_init(struct fifo *fifo, u_int16_t size, void *cb_data)
{
	if (size > sizeof(fifo->data))
		return -EINVAL;

	memset(fifo->data, 0, sizeof(fifo->data));
	fifo->size = size;
	fifo->producer = fifo->consumer = 0;
	fifo->watermark = 0;
	fifo->cb = cb;
	fifo->cb_data = cb_data;

	return 0;
}

