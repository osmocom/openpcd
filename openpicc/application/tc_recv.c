/***************************************************************
 *
 * OpenPICC - ISO 14443 Layer 2 Type A  T/C based receiver code
 * Implements a receiver using FDT Timer/Counter (TC2) and the
 * FIQ to measure the number of carrier cycles between modulation
 * pauses.
 * 
 * The timing measurements are given to the differential miller
 * decoder on the fly to interleave reception and decoding. This
 * means two things: a) The CPU will be held in an IRQ handler
 * with IRQs disabled for the time of reception and b) The frame
 * will already have been fully decoded to a iso14443_frame
 * structure when reception ends.
 * 
 * Copyright 2008 Henryk Plötz <henryk@ploetzli.ch>
 *
 ***************************************************************

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include <FreeRTOS.h>
#include <openpicc.h>
#include <errno.h>
#include <string.h>

#include <task.h>
#include <queue.h>

#include "tc_recv.h"

#include "iso14443a_diffmiller.h"
#include "usb_print.h"
#include "pio_irq.h"
#include "led.h"
#include "cmd.h"

struct tc_recv_handle {
	u_int8_t initialized;
	u_int8_t pauses_count;
	struct diffmiller_state *decoder;
	int current, next;
	tc_recv_callback_t callback;
	iso14443_frame *current_frame;
	xQueueHandle rx_queue;
};

static struct tc_recv_handle _tc;

#define BUFSIZE 1024
typedef struct {
	u_int32_t count;
	u_int32_t data[BUFSIZE];
} fiq_buffer_t;
fiq_buffer_t fiq_buffers[2];

fiq_buffer_t *tc_sniffer_next_buffer_for_fiq = 0;

iso14443_frame rx_frames[TC_RECV_NUMBER_OF_FRAME_BUFFERS];

#define REAL_FRAME_END 333 

static int tc_recv_buffer_overruns = 0;

static inline iso14443_frame *get_frame_buffer(tc_recv_handle_t th)
{
	if(th->current_frame) return th->current_frame;
	unsigned int i; iso14443_frame *result;
	for(i=0; i<sizeof(rx_frames)/sizeof(rx_frames[0]); i++) {
		if(rx_frames[i].state == FRAME_FREE) {
			result = &rx_frames[i];
			result->state = FRAME_PENDING;
			th->current_frame = result;
			return result;
		}
	}
	tc_recv_buffer_overruns++;
	return NULL;
}

static portBASE_TYPE handle_frame(iso14443_frame *frame, portBASE_TYPE task_woken)
{
	if(_tc.callback) _tc.callback(TC_RECV_CALLBACK_RX_FRAME_ENDED, frame);
	if(frame->state != FRAME_FREE) {
		task_woken = xQueueSendFromISR(_tc.rx_queue, &frame, task_woken);
	}
	_tc.current_frame = NULL;
	return task_woken;
}

static portBASE_TYPE handle_buffer(u_int32_t data[], unsigned int count, portBASE_TYPE task_woken)
{
	unsigned int offset = 0;
	while(offset < count) {
		iso14443_frame *rx_frame = get_frame_buffer(&_tc);
		if(rx_frame == NULL) return task_woken;
		int ret = iso14443a_decode_diffmiller(_tc.decoder, rx_frame, data, &offset, count);
		if(ret == 0) {
			task_woken = handle_frame(rx_frame, task_woken);
		}
	}
	return task_woken;
}

static inline portBASE_TYPE flush_buffer(fiq_buffer_t *buffer, portBASE_TYPE task_woken)
{
	if(buffer->count > 0) {
		if(buffer->count >= BUFSIZE) {
			usb_print_string_f("Warning: Possible buffer overrun detected\n\r",0);
			//overruns++;
		}
		buffer->count = MIN(buffer->count, BUFSIZE);
		task_woken = handle_buffer(buffer->data, buffer->count, task_woken);
		buffer->count = 0;
	}
	return task_woken;
}

#define NEXT_BUFFER(a) ((a+1)%(sizeof(fiq_buffers)/sizeof(fiq_buffers[0])))

static portBASE_TYPE switch_buffers(portBASE_TYPE task_woken)
{
	_tc.next = NEXT_BUFFER(_tc.current);
	task_woken = flush_buffer( &fiq_buffers[_tc.next] , task_woken);
	
	tc_sniffer_next_buffer_for_fiq = &fiq_buffers[_tc.current=_tc.next];
	return task_woken;
}

static portBASE_TYPE tc_recv_irq(u_int32_t pio, portBASE_TYPE task_woken)
{
	(void)pio;
	/* TODO There should be some emergency exit here to prevent the CPU from
	 * spinning in the IRQ for excessive amounts of time. (Maximum transmission
	 * time for 256 Byte frame is something like 21ms.)
	 */ 
	while(*AT91C_TC2_CV <= REAL_FRAME_END || 
			fiq_buffers[NEXT_BUFFER(_tc.current)].count > 0 || 
			fiq_buffers[_tc.current].count > 0) 
		task_woken = switch_buffers(task_woken);
	
	if(*AT91C_TC2_CV > REAL_FRAME_END) {
		iso14443_frame *rx_frame = get_frame_buffer(&_tc);
		if(rx_frame == NULL) return task_woken;
		int ret = iso14443a_diffmiller_assert_frame_ended(_tc.decoder, rx_frame);
		if(ret == 0) {
			task_woken = handle_frame(rx_frame, task_woken);
		}
	}
	return task_woken;
}


int tc_recv_init(tc_recv_handle_t *_th, int pauses_count, tc_recv_callback_t callback)
{
	if(_tc.initialized) return -EBUSY;
	tc_recv_handle_t th = &_tc;
	
	memset(fiq_buffers, 0, sizeof(fiq_buffers));
	th->current = th->next = 0;
	
	memset(rx_frames, 0, sizeof(rx_frames));
	th->current_frame = NULL;
	
	if(th->rx_queue == NULL) {
		th->rx_queue = xQueueCreate(TC_RECV_NUMBER_OF_FRAME_BUFFERS, sizeof(iso14443_frame*));
		if(th->rx_queue == NULL)
			return -ENOMEM;
	}
	
	th->pauses_count = pauses_count;
	th->decoder = iso14443a_init_diffmiller(th->pauses_count);
	if(!th->decoder) return -EBUSY;

	// The change interrupt is going to be handled by the FIQ and our secondary IRQ handler 
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_SSC_DATA);
	if( pio_irq_register(OPENPICC_SSC_DATA, &tc_recv_irq) < 0) 
		return -EBUSY;
	pio_irq_enable(OPENPICC_SSC_DATA);
	
	th->initialized = 1;
	*_th = th;
	
	th->callback = callback;
	if(th->callback) th->callback(TC_RECV_CALLBACK_SETUP, th);
	
	return 0;
}

int tc_recv_receive(tc_recv_handle_t th, iso14443_frame* *frame, unsigned int timeout)
{
	if(th == NULL) return -EINVAL;
	if(!th->initialized) return -EINVAL;
	
	if(xQueueReceive(th->rx_queue, frame, timeout)){
		if(*frame != NULL) return 0;
		else return -EINTR;
	}
	
	return -ETIMEDOUT;
}
