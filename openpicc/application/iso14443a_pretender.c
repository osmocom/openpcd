/***************************************************************
 *
 * OpenPICC - ISO 14443 Layer 3 response pretender, 
 * proof of concept
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
#include <board.h>
#include <task.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "openpicc.h"
#include "ssc_buffer.h"
#include "iso14443.h"
#include "iso14443_layer2a.h"
#include "iso14443a_pretender.h"
#include "iso14443a_manchester.h"
#include "usb_print.h"
#include "cmd.h"
extern volatile int fdt_offset;
#include "led.h"

struct challenge_response {
	u_int8_t UID[5];
	u_int8_t nonce[4];
	u_int8_t waiting_for_response; 
	struct {
		u_int8_t data[8];
		u_int8_t parity[2];
		size_t len;
	} response;
};

struct challenge_response challenge_response;

static const iso14443_frame ATQA_FRAME = {
	TYPE_A,
	FRAME_PREFILLED,
	{{STANDARD_FRAME, PARITY, ISO14443A_LAST_BIT_NONE, CRC_UNCALCULATED}},
	2,
	0, 0,
	{4, 0},
	{}
};

static const iso14443_frame ATS_FRAME = {
	TYPE_A,
	FRAME_PREFILLED,
	{{STANDARD_FRAME, PARITY, ISO14443A_LAST_BIT_NONE, CRC_UNCALCULATED}},
	3,
	0, 0,
	{0x08, 0xB6, 0xDD},
	{}
};

static iso14443_frame UID_FRAME, NONCE_FRAME;
//static u_int8_t UID[]   = {0xF4, 0xAC, 0xF9, 0xD7}; // bcc = 0x76
static u_int8_t UID[]   = {0x00, 0x00, 0x00, 0x00}; 
static u_int8_t nonce[] = {0x00, 0x00, 0x00, 0x00};

#define FRAME_SIZE(bytes) (2* (1+(9*bytes)+1) )
#define SIZED_BUFFER(bytes) struct { int len; u_int8_t data[FRAME_SIZE(bytes)]; }
#define BYTES_AND_BITS(bytes,bits) (bytes*9+bits)

static ssc_dma_tx_buffer_t ATQA_BUFFER, UID_BUFFER, ATS_BUFFER, NONCE_BUFFER;

static void fast_receive_callback(ssc_dma_rx_buffer_t *buffer, iso14443_frame *frame, u_int8_t in_irq)
{
	(void)buffer; (void)in_irq;
	u_int32_t cv = *AT91C_TC2_CV;
	
	ssc_dma_tx_buffer_t *tx_buffer=NULL;
	int fdt = 0;
	
	switch(frame->parameters.a.last_bit) {
	case ISO14443A_LAST_BIT_0:
		fdt = 20;
		break;
	case ISO14443A_LAST_BIT_1:
		fdt = 84;
		break;
	case ISO14443A_LAST_BIT_NONE:
		fdt = 0;
		break;
	}
	
	
	if(cv < 870) /* Anticollision is time-critical, do not even try to send if we're too late anyway */
		switch(BYTES_AND_BITS(frame->numbytes,frame->numbits)) {
		case BYTES_AND_BITS(0, 7): /* REQA or WUPA (7 bits) */
			if(frame->data[0] == 0x26 || frame->data[0] == 0x52)
				tx_buffer = &ATQA_BUFFER;
			fdt += 9*128;
			break;
		case BYTES_AND_BITS(2, 0): /* ANTICOL (2 bytes) */
			if(frame->data[0] == 0x93 && frame->data[1] == 0x20)
				tx_buffer = &UID_BUFFER;
			fdt += 9*128;
			break;
		case BYTES_AND_BITS(9, 0): /* SELECT (9 bytes) */
			if(frame->data[0] == 0x93 && frame->data[1] == 0x70 && frame->parameters.a.crc &&
					( *((u_int32_t*)&frame->data[2]) ==  *((u_int32_t*)&UID) ) )
				tx_buffer = &ATS_BUFFER;
			fdt += 9*128;
			break;
		}
	
	if(tx_buffer == NULL) 
		switch(BYTES_AND_BITS(frame->numbytes, frame->numbits)) {
		case BYTES_AND_BITS(4, 0):
			if( (frame->data[0] & 0xfe) == 0x60 && frame->parameters.a.crc) {
				/* AUTH1A or AUTH1B */
				int ret = manchester_encode(NONCE_BUFFER.data,  sizeof(NONCE_BUFFER.data),  &NONCE_FRAME);
				if(ret < 0) {
				} else {
					NONCE_BUFFER.len = ret;
					tx_buffer = &NONCE_BUFFER;
				}
				
				/*fdt += ((cv / 128) + 10) * 128;*/
				fdt += 50*128;  /* 52 ok, 26 not, 39 not, 45 not, 48 not, 50 ok, 49 not */
			}
			break;
		}
	
	/* Add some extra room to the fdt for testing */
	//fdt += 3*128;
	fdt += fdt_offset;
	
	if(tx_buffer != NULL) {
		tx_buffer->state = SSC_FULL;
		if(	iso14443_transmit(tx_buffer, fdt, 1, 0) < 0) {
			usb_print_string_f("Tx failed ", 0);
		}
	}
	
#if 0
	u_int32_t cv2 = *AT91C_TC2_CV;
	usb_print_string_f("\r\n",0);
	if(tx_buffer == &NONCE_BUFFER) usb_print_string_f("---> ",0);
	int old=usb_print_set_default_flush(0);
	DumpUIntToUSB(cv);
	DumpStringToUSB(":");
	DumpUIntToUSB(cv2);
	usb_print_set_default_flush(old);
#endif
	
	switch(BYTES_AND_BITS(frame->numbytes,frame->numbits)) {
	case BYTES_AND_BITS(4, 0):
		if(frame->parameters.a.crc && frame->data[0] == 0x50 && frame->data[1] == 0x00) {
			/* HLTA */
			UID[3]++;
			if(UID[3]==0) {
				UID[2]++;
				if(UID[2]==0) {
					UID[1]++;
					if(UID[1]==0) {
						UID[0]++;
					}
				}
			}
			set_UID(UID, sizeof(UID));
		}
	break;
	}
	
	if(tx_buffer) usb_print_string_f("\r\n",0);
	if(tx_buffer == &UID_BUFFER) {
		memcpy(&challenge_response.UID, UID_FRAME.data, 5);
		usb_print_string_f("uid", 0);
	} else if(tx_buffer == &NONCE_BUFFER) {
		memcpy(&challenge_response.nonce, NONCE_FRAME.data, 4);
		challenge_response.waiting_for_response = 1;
		usb_print_string_f("nonce", 0);
	} else if(challenge_response.waiting_for_response) {
		challenge_response.waiting_for_response = 0;
		if(frame->numbytes != 8) {
			usb_print_string_f("tilt ",0);
		}
		challenge_response.response.len = frame->numbytes + (frame->numbits+7)/8;
		memcpy(&challenge_response.response.data, frame->data, challenge_response.response.len);
		memcpy(&challenge_response.response.parity, frame->parity, 1);

		int old=usb_print_set_default_flush(0);
		DumpStringToUSB("[[");
		DumpBufferToUSB((char*)challenge_response.UID, 5);
		DumpStringToUSB(" ");
		DumpBufferToUSB((char*)challenge_response.nonce, 4);
		DumpStringToUSB(" ");
		DumpBufferToUSB((char*)challenge_response.response.data, challenge_response.response.len);
		DumpStringToUSB("]]");
		usb_print_set_default_flush(old);
	}
	//usb_print_string_f("%", 0);
}

static void prepare_frame(iso14443_frame *frame, int len)
{
	memset(frame, 0, sizeof(*frame));
	frame->type = TYPE_A;
	frame->parameters.a.format = STANDARD_FRAME;
	frame->parameters.a.parity = PARITY;
	frame->parameters.a.last_bit = ISO14443A_LAST_BIT_NONE;
	frame->parameters.a.crc = CRC_UNCALCULATED;
	
	frame->numbytes = len;
}

int set_UID(u_int8_t *uid, size_t len)
{
	prepare_frame(&UID_FRAME, len+1);
	
	u_int8_t bcc = 0;
	unsigned int i;
	for(i=0; i<len; i++) {
		UID_FRAME.data[i] = uid[i];
		bcc ^= uid[i];
	}
	UID_FRAME.data[i] = bcc;
	UID_FRAME.state = FRAME_PREFILLED;
	memcpy(UID, uid, len);
	
	memset(&UID_BUFFER, 0, sizeof(UID_BUFFER));
	int ret = manchester_encode(UID_BUFFER.data,  sizeof(UID_BUFFER.data),  &UID_FRAME);
	if(ret < 0) return ret;
	else UID_BUFFER.len = ret;
	return 0;
}

int get_UID(u_int8_t *uid, size_t len)
{
	if(len < 4 || len > 4) return -1;
	memcpy(uid, UID, len);
	return 0;
}

int set_nonce(u_int8_t *_nonce, size_t len)
{
	prepare_frame(&NONCE_FRAME, len);
	
	memcpy(&NONCE_FRAME.data, _nonce, len);
	NONCE_FRAME.state = FRAME_PREFILLED;
	memcpy(nonce, _nonce, len);
	
	memset(&NONCE_BUFFER, 0, sizeof(NONCE_BUFFER));
	int ret = manchester_encode(NONCE_BUFFER.data,  sizeof(NONCE_BUFFER.data),  &NONCE_FRAME);
	if(ret < 0) return ret;
	else NONCE_BUFFER.len = ret;
	return 0;
}

int get_nonce(u_int8_t *_nonce, size_t len)
{
	if(len < 4 || len > 4) return -1;
	memcpy(_nonce, nonce, len);
	return 0;
}

void iso14443a_pretender (void *pvParameters)
{
	(void)pvParameters;
	int res;
	
	/* Delay until USB print etc. are ready */
	int i;
	for(i=0; i<=1000; i++) {
		vLedSetBrightness(LED_RED, i);
		vLedSetBrightness(LED_GREEN, i);
		vTaskDelay(1*portTICK_RATE_MS);
	}
	
	for(i=0; i<=1000; i+=4) {
		vLedSetBrightness(LED_GREEN, 1000-i);
		vTaskDelay(1*portTICK_RATE_MS);
	}
	
	int ret;
#define PREFILL_BUFFER(dst, src) ret = manchester_encode(dst.data,  sizeof(dst.data),  &src); \
	if(ret < 0) goto prefill_failed; else dst.len = ret;
	
	PREFILL_BUFFER(ATQA_BUFFER,  ATQA_FRAME);
	//PREFILL_BUFFER(UID_BUFFER,   UID_FRAME);
	PREFILL_BUFFER(ATS_BUFFER,   ATS_FRAME);
	//PREFILL_BUFFER(NONCE_BUFFER, NONCE_FRAME);
	
	set_UID(UID, sizeof(UID));
	set_nonce(nonce, sizeof(nonce));
	
	if(0) {
prefill_failed:
		usb_print_string("Buffer prefilling failed\n\r");
		while(1) {
			for(i=1000; i<=3000; i++) {
				vLedSetBrightness(LED_GREEN, abs(1000-(i%2000)));
				vTaskDelay(1*portTICK_RATE_MS);
			}
		}
	}
	
	do {
		res = iso14443_layer2a_init(1);
		if(res < 0) {
			usb_print_string("Pretender: Initialization failed: -");
			DumpUIntToUSB(-res);
			usb_print_string("\n\r");
			for(i=0; i<=10000; i++) {
				vLedSetBrightness(LED_RED, abs(1000-(i%2000)));
				vTaskDelay(1*portTICK_RATE_MS);
			}
		}
	} while(res < 0);
	
	
	usb_print_string("Waiting for carrier. ");
	while(iso14443_wait_for_carrier(1000 * portTICK_RATE_MS) != 0) {
	}
	usb_print_string("Carrier detected.\n\r");
	
	for(i=0; i<=1000; i+=4) {
		vLedSetBrightness(LED_RED, 1000-i);
		vTaskDelay(1*portTICK_RATE_MS);
	}
	
	vTaskDelay(250*portTICK_RATE_MS);
	vLedSetGreen(0);
	vLedSetRed(0);
	
	int current_detected = 0, last_detected = 0;
	portTickType last_switched = 0;
#define DETECTED_14443A_3 1
#define DETECTED_14443A_4 2
#define DETECTED_MIFARE 4
	
	while(true) {
		iso14443_frame *frame = 0;
		res = iso14443_receive(fast_receive_callback, &frame, 1000 * portTICK_RATE_MS);
		if(res >= 0) {
			switch(BYTES_AND_BITS(frame->numbytes, frame->numbits) ) {
			case BYTES_AND_BITS(0, 7): /* REQA (7 bits) */
				current_detected |= DETECTED_14443A_3;
				break;
			case BYTES_AND_BITS(2, 0): /* ANTICOL (2 bytes) */
			case BYTES_AND_BITS(9, 0): /* SELECT (9 bytes) */
				current_detected |= DETECTED_14443A_3;
				break;
			case BYTES_AND_BITS(4, 0): /* AUTH1A (or any other four byte frame) */
			case BYTES_AND_BITS(8, 0): /* AUTH2 (8 bytes) */
				current_detected |= DETECTED_MIFARE;
				break;
			}
			
			portENTER_CRITICAL();
			frame->state = FRAME_FREE;
			portEXIT_CRITICAL();
		} else {
			if(res != -ETIMEDOUT) {
				usb_print_string("Receive error: ");
				switch(res) {
					case -ENETDOWN:   usb_print_string("PLL is not locked or PLL lock lost\n\r"); break;
					case -EBUSY:      usb_print_string("A Tx is currently running or pending, can't receive\n\r"); break;
					case -EALREADY:   usb_print_string("There's already an iso14443_receive() invocation running\n\r"); break;
				}
				vTaskDelay(1000 * portTICK_RATE_MS); // FIXME Proper error handling, e.g. wait for Tx end in case of EBUSY
			} else if(0) {
				DumpStringToUSB("\n\r");
				DumpTimeToUSB(xTaskGetTickCount());
				usb_print_string(": -- Mark --");
			}
		}
		
		if( last_switched < xTaskGetTickCount()-(1000*portTICK_RATE_MS) ) {
			last_detected = current_detected;
			current_detected = 0;
			last_switched = xTaskGetTickCount();
		}

#if 0
		if(last_detected & (DETECTED_14443A_3 | DETECTED_14443A_4 | DETECTED_MIFARE)) {
			if(last_detected & DETECTED_MIFARE) {
				vLedSetGreen(0);
				vLedSetRed(1);
			} else if(last_detected & DETECTED_14443A_4) {
				vLedSetGreen(1);
				vLedSetRed(0);
			} else {
				vLedSetGreen(1);
				vLedSetRed(1);
			}
		} else {
			vLedSetGreen(0);
			vLedSetRed(0);
		}
#endif
	}
}
