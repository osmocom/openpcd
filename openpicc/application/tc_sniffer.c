/***************************************************************
 *
 * OpenPICC - T/C based dumb sniffer
 * 
 * TC2 will reset its value on each falling edge of SSC_DATA, we
 * will have the FIQ store the TC2 value on each rising edge of
 * SSC_DATA. This will give us a count of carrier cycles between
 * modulation pauses which should be enough information to decode
 * the modified miller encoding.
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
#include <task.h>
#include <string.h>

#include <USB-CDC.h>

#include "tc_sniffer.h"
#include "load_modulation.h"
#include "pll.h"
#include "tc_fdt.h"
#include "usb_print.h"
#include "cmd.h"
#include "pio_irq.h"

/* Problem: We want to receive data from the FIQ without locking (the FIQ must not be blocked ever)
 * Strategy: Double buffering.
 */

#define BUFSIZE 1024
#define WAIT_TICKS (20*portTICK_RATE_MS)
typedef struct {
	u_int32_t count;
	u_int32_t data[BUFSIZE];
} fiq_buffer_t;
fiq_buffer_t fiq_buffers[2];

fiq_buffer_t *tc_sniffer_next_buffer_for_fiq = 0;

portBASE_TYPE currently_sniffing = 0;
enum { NONE, REQUEST_START, REQUEST_STOP } request_change = NONE;

#define MIN(a, b) ((a)>(b)?(b):(a))
void flush_buffer(fiq_buffer_t *buffer)
{
	/* Write all data from the given buffer out, then zero the count */
	if(buffer->count > 0) {
		vUSBSendBuffer_blocking((unsigned char*)(&(buffer->data[0])), 0, MIN(buffer->count,BUFSIZE)*4, WAIT_TICKS);
		if(buffer->count >= BUFSIZE)
			vUSBSendBuffer_blocking((unsigned char*)"////", 0, 4, WAIT_TICKS);
		else
			vUSBSendBuffer_blocking((unsigned char*)"____", 0, 4, WAIT_TICKS);
		buffer->count = 0;
	}
}

void start_stop_sniffing(void)
{
	if(currently_sniffing)
		request_change = REQUEST_STOP;
	else
		request_change = REQUEST_START;
}

void tc_sniffer (void *pvParameters)
{
	(void)pvParameters;
	
	/* Disable load modulation circuitry
	 * (Must be done explicitly, because the default state is pull-up high, leading
	 * to a constant modulation output which prevents reception. I've been bitten by
	 * this more than once.)
	 */
	load_mod_init();
	load_mod_level(0);
	
	pll_init();
	pll_inhibit(0);
	
	tc_fdt_init();
	
	memset(fiq_buffers, 0, sizeof(fiq_buffers));
	
	/* Wait for the USB and CMD threads to start up */
	vTaskDelay(1000 * portTICK_RATE_MS);
	
	// The change interrupt is going to be handled by the FIQ 
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_SSC_DATA);
	pio_irq_enable(OPENPICC_SSC_DATA);
	
	int current = 0;
	while(1) {
		/* Main loop of the sniffer */
		
		if(currently_sniffing) {
			int next = (current+1)%(sizeof(fiq_buffers)/sizeof(fiq_buffers[0]));
			flush_buffer( &fiq_buffers[next] );
			/* The buffer designated by next is now empty, give it to the fiq,
			 * we'll just guess that this write is atomic */ 
			tc_sniffer_next_buffer_for_fiq = &fiq_buffers[current=next];

			if(request_change == REQUEST_STOP) {
				currently_sniffing = 0;
				request_change = NONE;
				
				tc_sniffer_next_buffer_for_fiq = 0;
				memset(fiq_buffers, 0, sizeof(fiq_buffers));
				current = 0;
				vUSBSendBuffer_blocking((unsigned char *)"----", 0, 4, WAIT_TICKS);
				usb_print_set_force_silence(0);
			} else vTaskDelay(2* portTICK_RATE_MS);
		} else {
			// Do nothing, wait longer
			
			if(request_change == REQUEST_START) {
				// Prevent usb_print code from barging in
				usb_print_set_force_silence(1);
				vUSBSendBuffer_blocking((unsigned char *)"----", 0, 4, WAIT_TICKS);
				currently_sniffing = 1;
				request_change = NONE;
			} else vTaskDelay(100 * portTICK_RATE_MS);
		}
	}
}
