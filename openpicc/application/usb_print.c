/* Buffered USB debug output
 * (C) 2007 Henryk Plötz <henryk@ploetzli.ch>
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

#include <FreeRTOS.h>
#include <task.h> 
#include <semphr.h>
#include <USB-CDC.h>
#include <string.h>

#include "usb_print.h"

#define BUFLEN (2*1024)

static char ringbuffer[BUFLEN];
static int ringstart, ringstop;
static int default_flush = 1;
static xSemaphoreHandle print_semaphore;

void usb_print_buffer(const char* buffer, int start, int stop) {
	usb_print_buffer_f(buffer,start,stop,default_flush);
}
int usb_print_buffer_f(const char* buffer, int start, int stop, int flush)
{
	int pos=start, endpos=stop;
	while(pos<stop) {
		int available = BUFLEN-1 - (((ringstop+BUFLEN)-ringstart) % BUFLEN);
		if(available == 0 && flush) { 
			usb_print_flush();
			continue;
		}
		if((endpos-pos)>available) endpos=pos+available;
		
		while(pos < endpos) {
			ringbuffer[ringstop] = buffer[pos];
			pos++;
			available--;
			ringstop = (ringstop+1) % BUFLEN;
		}
		
		if(flush) usb_print_flush();
		else if(available==0)
			return 0;
		endpos=stop;
		if(pos>=stop)
			return (available > 0);
	}
	return 0;
}

void usb_print_string(const char *string) {
	usb_print_string_f(string, default_flush);
}
int usb_print_string_f(const char* string, int flush)
{
	int start = 0, stop = strlen(string);
	return usb_print_buffer_f(string, start, stop, flush);
}

void usb_print_char(const char c) {
	usb_print_char_f(c, default_flush);
}
int usb_print_char_f(const char c, int flush)
{
	return usb_print_buffer_f(&c, 0, 1, flush);
}

int usb_print_get_default_flush(void)
{
	return default_flush;
}

int usb_print_set_default_flush(int flush)
{
	int old_flush = default_flush;
	default_flush = flush;
	return old_flush;
}


/* Must NOT be called from ISR context */
void usb_print_flush(void)
{
	int oldstop, newstart;
	taskENTER_CRITICAL();
	if(print_semaphore == NULL)
		usb_print_init();
	if(print_semaphore == NULL) {
		taskEXIT_CRITICAL();
		return;
	}
	taskEXIT_CRITICAL();
	
	xSemaphoreTake(print_semaphore, portMAX_DELAY);
	
	taskENTER_CRITICAL();
	oldstop = ringstop;
	newstart = ringstart;
	taskEXIT_CRITICAL();
	 
	while(newstart != oldstop) {
		vUSBSendByte(ringbuffer[newstart]);
		newstart = (newstart+1) % BUFLEN;
	}
	
	taskENTER_CRITICAL();
	ringstart = newstart;
	taskEXIT_CRITICAL();
	
	xSemaphoreGive(print_semaphore);
}

void usb_print_init(void)
{
	memset(ringbuffer, 0, BUFLEN);
	ringstart = ringstop = 0;
	vSemaphoreCreateBinary( print_semaphore );
}
