/* ISO14443A Manchester encoder for OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
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


/*
 * Definitions for 106kBps, at sampling clock 1695kHz
 *
 * 		bit sample pattern for one bit cycle
 * 		MSB first		LSB first	hex LSB first
 * Sequence D	1010101000000000	0000000001010101	0x0055
 * Sequence E	0000000010101010	0101010100000000	0x5500
 * Sequence F	0000000000000000	0000000000000000	0x0000
 *
 * Logic 1	Sequence D
 * Logic 0	Sequence E
 * SOF		Sequence D
 * EOF		Sequence F
 *
 * 212/424/848kBps: BPSK.
 *
 * SOF: 32 subcarrier clocks + bit '0'
 *
 * SOF:		hex LSB first: 0x55555555 55555555 + bit '0'
 *
 * EOF:		even parity of last byte (!)
 *
 */

#define MANCHESTER_SEQ_D	0x0055
#define MANCHESTER_SEQ_E	0x5500
#define MANCHESTER_SEQ_F	0x0000

#include <errno.h>
#include <string.h>
#include "openpicc.h"
#include "iso14443_layer3a.h"
#include "iso14443a_manchester.h"

enum parity {
	PARITY_NONE, /* Don't add a parity bit */
	ODD_PARITY, EVEN_PARITY, /* Calculate parity */
	PARITY_0, PARITY_1 /* Set fixed parity */
};

static void manchester_enc_byte(u_int16_t **s16, u_int8_t data, enum parity parity)
{
	int i;
	u_int8_t sum_1 = 0;
	u_int16_t *samples16 = *s16;

	/* append 8 sample blobs, one for each bit */
	for (i = 0; i < 8; i++) {
		if (data & (1 << i)) {
			*(samples16) = MANCHESTER_SEQ_D;
			sum_1++;
		} else {
			*(samples16) = MANCHESTER_SEQ_E;
		}
		samples16++;
	}
	if(parity != PARITY_NONE) {
		/* Append parity */
		u_int8_t par=0;
		switch(parity) {
			case PARITY_NONE: break;
			case PARITY_0: par = 0; break;
			case PARITY_1: par = 1; break;
			case ODD_PARITY:  par = (sum_1 & 0x1) ? 0 : 1; break;
			case EVEN_PARITY: par = (sum_1 & 0x1) ? 1 : 0; break;
		}
		if (par)
			*(samples16) = MANCHESTER_SEQ_D;
		else
			*(samples16) = MANCHESTER_SEQ_E;
		samples16++;
	}
	*s16 = samples16;
}

int manchester_encode(u_int8_t *sample_buf, u_int16_t sample_buf_len, 
		      const iso14443_frame *frame)
{
	unsigned int i, enc_size;
	u_int16_t *samples16;
	
	if(frame->type != TYPE_A) return -EINVAL;
	if(frame->parameters.a.format != STANDARD_FRAME) return -EINVAL; /* AC not implemented yet */
	
	/* One bit data is 16 bit is 2 byte modulation data */
	enc_size = 2*2 /* SOF and EOF */
		+ frame->numbytes * 8 * 2
		+ ((frame->parameters.a.parity != NO_PARITY) ? 1 : 0)*8*2
		+ 6;

	if (sample_buf_len < enc_size)
		return -EINVAL;
	
	memset(sample_buf, 0, enc_size);
	
	samples16 = (u_int16_t*)sample_buf;
	(*samples16) = 5;
	samples16+=2; // SSC workaround
	//*(samples16++) = 0xb;

	/* SOF */
	*(samples16++) = MANCHESTER_SEQ_D;
	
	if(frame->parameters.a.parity == NO_PARITY)
		for (i = 0; i < frame->numbytes; i++)
			manchester_enc_byte(&samples16, frame->data[i], PARITY_NONE);
	else if(frame->parameters.a.parity == GIVEN_PARITY)
		for (i = 0; i < frame->numbytes; i++)
			manchester_enc_byte(&samples16, frame->data[i], 
				(frame->parity[i/8]&(1<<(i%8))) ?PARITY_1:PARITY_0);
	else if(frame->parameters.a.parity == PARITY)
		for (i = 0; i < frame->numbytes; i++)
			manchester_enc_byte(&samples16, frame->data[i], ODD_PARITY);
		
	/* EOF */
	*(samples16++) = MANCHESTER_SEQ_F;

	return enc_size;
}

#if 0
/* Broken? */
#define BPSK_SPEED_212	


static u_int32_t bpsk_sample_size(u_int8_t frame_bytelen);

int bpsk_encode(char *sample_buf, u_int16_t sample_buf_len,
		const char *data, u_int8_t data_len)
{
	/* burst of 32 sub carrier cycles */	
	memset(sample_buf, 0x55, 8);

}
#endif
