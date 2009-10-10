/***************************************************************
 *
 * OpenPICC - T/C based Legic Prime emulator
 * 
 * TC2 will reset its value on each falling edge of SSC_DATA, we
 * will have the FIQ store the TC2 value on each rising edge of
 * SSC_DATA. This will give us a count of carrier cycles between
 * modulation pauses which is enough information to trivially
 * decode the pulse-pause-modulation
 *
 * Copyright 2008-2009 Henryk Plötz <henryk@ploetzli.ch>
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
#include <task.h>
#include <string.h>

#include <USB-CDC.h>

#include "tc_sniffer.h"
#include "tc_legic.h"
#include "load_modulation.h"
#include "pll.h"
#include "led.h"
#include "tc_fdt.h"
#include "usb_print.h"
#include "cmd.h"
#include "pio_irq.h"

struct legic_frame {
	int num_bytes;
	int num_bits;
	char data[10];
} current_frame;

static void frame_append_bit(struct legic_frame *f, int bit)
{
	if(f->num_bytes >= (int)sizeof(f->data))
		return; /* Overflow, won't happen */
	f->data[f->num_bytes] |= (bit<<f->num_bits);
	f->num_bits++;
	if(f->num_bits > 7) {
		f->num_bits = 0;
		f->num_bytes++;
	}
}

static int frame_is_empty(struct legic_frame *f)
{
	return( (f->num_bytes + f->num_bits) == 0 );
}

static void frame_handle(struct legic_frame *f)
{
	int i;
	vLedSetGreen(1);
	for(i=0; i< (f->num_bytes*8+f->num_bits); i++) {
		if( f->data[i/8] & (1<<(i%8)) )
			DumpStringToUSB("1");
		else
			DumpStringToUSB("0");
	}
	vLedSetGreen(0);
	if( !frame_is_empty(f) ) 
		DumpStringToUSB("\n\r");
}

static void frame_clean(struct legic_frame *f)
{
	if(!frame_is_empty(f)) 
		memset(f->data, 0, sizeof(f->data));
	f->num_bits = 0;
	f->num_bytes = 0;
}

static void emit(int bit)
{
	if(bit == -1) {
		frame_handle(&current_frame);
		frame_clean(&current_frame);
	} else if(bit == 0) {
		frame_append_bit(&current_frame, 0);
	} else if(bit == 1) {
		frame_append_bit(&current_frame, 1);
	}
}

#define MIN(a, b) ((a)>(b)?(b):(a))
static void flush_buffer(tc_fiq_buffer_t *buffer)
{
	/* Process the data */
	if(buffer->count > 0) {
		unsigned int i;
		for(i=0; i<buffer->count; i++) {
			if(buffer->data[i] > 850 && buffer->data[i] < 950) {
				emit(1);
			} else if(buffer->data[i] > 450 && buffer->data[i] < 550) {
				emit(0);
			} else emit(-1);
		}
	}
	if(tc_fdt_get_current_value() > 1356 && !frame_is_empty(&current_frame)) {
		emit(-1);
	}
}

void tc_legic (void *pvParameters)
{
	(void)pvParameters;
	
	tc_fiq_setup();
	
	/* Wait for the USB and CMD threads to start up */
	vTaskDelay(1000 * portTICK_RATE_MS);

	tc_fiq_start();
	
	while(1) {
		/* Main loop of the sniffer */
		tc_fiq_process(flush_buffer);
	}
		
}
