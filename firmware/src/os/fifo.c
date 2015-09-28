/* Implementation of a virtual FIFO for OpenPCD
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
 */


#include "fifo.h"

#include <errno.h>
#include <string.h>

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
		fifo->irq |= FIFO_IRQ_LO;
	else
		fifo->irq &= FIFO_IRQ_LO;

	if (fifo->size - avail >= fifo->watermark)
		fifo->irq |= FIFO_IRQ_HI;
	else
		fifo->irq &= FIFO_IRQ_HI;
}

void fifo_check_raise_int(struct fifo *fifo)
{
	if (fifo->irq & fifo->irq_en)
		fifo->callback(fifo, fifo->irq, fifo->cb_data);
}


uint16_t fifo_data_put(struct fifo *fifo, uint16_t len, uint8_t *data)
{
	if (len > fifo_available(fifo)) {
		len = fifo_available(fifo);
		fifo->irq |= FIFO_IRQ_OFLOW;
	}

	if (len + fifo->producer <= fifo->size) {
		/* easy case */
		memcpy(&fifo->data[fifo->producer], data, len);
		fifo->producer += len;
	} else {
		/* difficult: wrap around */
		uint16_t chunk_len;

		chunk_len = fifo->size - fifo->producer;
		memcpy(&fifo->data[fifo->producer], data, chunk_len);

		memcpy(&fifo->data[0], data + chunk_len, len - chunk_len);
		fifo->producer = len - chunk_len;
	}

	fifo_check_water(fifo);

	return len;
}


uint16_t fifo_data_get(struct fifo *fifo, uint16_t len, uint8_t *data)
{
	uint16_t avail = fifo_available(fifo);

	if (avail < len)
		len = avail;

	if (fifo->producer > fifo->consumer) {
		/* easy case */
		memcpy(data, &fifo->data[fifo->consumer], len);
	} else {
		/* difficult case: wrap */
		uint16_t chunk_len = fifo->size - fifo->consumer;
		memcpy(data, &fifo->data[fifo->consumer], chunk_len);
		memcpy(data+chunk_len, &fifo->data[0], len - chunk_len);
	}

	fifo_check_water(fifo);

	return len;
}

int fifo_init(struct fifo *fifo, uint16_t size, 
	      void (*cb)(struct fifo *fifo, uint8_t event, void *data), void *cb_data)
{
	if (size > sizeof(fifo->data))
		return -EINVAL;

	memset(fifo->data, 0, sizeof(fifo->data));
	fifo->size = size;
	fifo->producer = fifo->consumer = 0;
	fifo->watermark = 0;
	fifo->callback = cb;
	fifo->cb_data = cb_data;

	return 0;
}

