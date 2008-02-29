/* ISO14443A Manchester encoder for OpenPICC
 * (C) 2007 by Henryk Plötz <henryk@ploetzli.ch>
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

#include <openpicc.h>
#include <FreeRTOS.h>

#include <string.h>

#include "iso14443_layer3a.h"
#include "usb_print.h"
#include "ssc_buffer.h"
#include "cmd.h"

#if 0
// With debugging
#define MILLER_DEBUG_STRING DumpStringToUSB
#define MILLER_DEBUG_UINT DumpUIntToUSB 
#else
// Without debugging
#define MILLER_DEBUG_STRING(...) if(0){(void)__VA_ARGS__;}
#define MILLER_DEBUG_UINT(...) if(0){(void)__VA_ARGS__;} 
#endif

#ifdef FOUR_TIMES_OVERSAMPLING
#define OVERSAMPLING_RATE	4

/* definitions for four-times oversampling */
#define SEQ_X	0x4
#define SEQ_Y	0x0
#define SEQ_Z	0x1
#else
#define OVERSAMPLING_RATE	2
#define SEQ_X   0x2
#define SEQ_Y   0x0
#define SEQ_Z   0x1
#endif

enum miller_sequence {
	SEQUENCE_X,
	SEQUENCE_Y,
	SEQUENCE_Z,
};

#define BIT_ENDMARKER -1

static const int BITSAMPLE_MASK = ~(~0 << OVERSAMPLING_RATE);

int iso14443a_decode_miller(iso14443_frame *frame, 
	const ssc_dma_rx_buffer_t * const buffer)
{
	u_int32_t i,j;
	signed int bit = 0, last_bit = ISO14443A_LAST_BIT_NONE, next_to_last_bit = ISO14443A_LAST_BIT_NONE;
	enum miller_sequence current_seq;
	unsigned int bitpos = 0;
	
	memset(frame, 0, sizeof(*frame));
	frame->type = TYPE_A;
	frame->parameters.a.parity = GIVEN_PARITY;
	
	u_int32_t sample = 0;
	u_int8_t *sample_8=0;
	u_int16_t *sample_16=0;
	u_int32_t *sample_32=0;
	
	switch(buffer->reception_mode->transfersize_pdc) {
		case 8:  sample_8 =  ((u_int8_t*)buffer->data);  break;
		case 16: sample_16 = ((u_int16_t*)buffer->data); break;
		case 32: sample_32 = ((u_int32_t*)buffer->data); break;
	}
	
	for(i=0; i<buffer->len_transfers && bit != BIT_ENDMARKER; i++) {
		MILLER_DEBUG_STRING(" ");
		switch(buffer->reception_mode->transfersize_pdc) {
			case 8:  sample = *sample_8++;  break;
			case 16: sample = *sample_16++; break;
			case 32: sample = *sample_32++; break;
		}
		MILLER_DEBUG_UINT(sample);
		
		for(j=0; j<buffer->reception_mode->transfersize_ssc && bit != BIT_ENDMARKER; j+=OVERSAMPLING_RATE) {
			MILLER_DEBUG_STRING(".");
			int bitsample = (sample>>j) & BITSAMPLE_MASK;
			switch(bitsample) {
				case SEQ_X: current_seq = SEQUENCE_X; MILLER_DEBUG_STRING("X"); break;
				case SEQ_Y: current_seq = SEQUENCE_Y; MILLER_DEBUG_STRING("Y"); break;
				case SEQ_Z: current_seq = SEQUENCE_Z; MILLER_DEBUG_STRING("Z"); break;
				default: MILLER_DEBUG_UINT(bitsample); current_seq = SEQUENCE_Y;
			}
			
			switch(current_seq) {
				case SEQUENCE_X:
					bit = 1; break;
				case SEQUENCE_Y: /* Fall-through to SEQUENCE_Z */
					if(last_bit == 0) {
						bit = BIT_ENDMARKER;
						MILLER_DEBUG_STRING("!");
						break;
					}
				case SEQUENCE_Z:
					bit = 0; break;
			}
			
			switch(bit) {
				case BIT_ENDMARKER:
					bitpos-=2; /* Subtract this sequence and the previous sequence (which was a 0) */
					frame->parameters.a.last_bit = next_to_last_bit;
					break;
				case 0: /* Fall-through */
				case 1: {
					int bytepos = bitpos/9;
					if(bitpos % 9 == 8) { /* Parity bit */
						frame->parity[ bytepos/8 ] |= (bit<<(bytepos%8));
					} else {
						frame->data[ bytepos ] |= (bit<<(bitpos%9));
					}
				}
			}
			
			next_to_last_bit = last_bit;
			last_bit = bit;
			bitpos++;
		}
	}
	
	frame->numbytes = bitpos/9;
	frame->numbits = bitpos%9;
	MILLER_DEBUG_STRING(" ");
	MILLER_DEBUG_UINT(frame->numbytes);
	MILLER_DEBUG_STRING(" bytes, ");
	MILLER_DEBUG_UINT(frame->numbits);
	MILLER_DEBUG_STRING(" bits ");
	
	return 0;
}
