/* 
 * ISO14443A modified Miller decoder for OpenPICC
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

/*
 * 		LSB First	LSB 	hex
 * Sequence X	0010		0100	0x4
 * Sequence Y	0000		0000	0x0
 * Sequence Z	1000		0001	0x1
 *
 * Logic 1	Sequence X
 * Logic 0	Sequence Y with two exceptions:
 * 		- if there are more contiguous 0, Z used from second one
 * 		- if the first bit after SOF is 0, sequence Z used for all contig 0's
 * SOF		Sequence Z
 * EOF		Logic 0 followed by Sequence Y
 *
 * cmd	   hex	   bits            symbols              hex (quad-sampled)
 *
 * REQA    0x26    S 0110010 E     Z ZXXYZXY ZY		0x10410441
 * WUPA    0x52    S 0100101 E     Z ZXYZXYX YY		0x04041041
 *
 * SOF is 'eaten' by SSC start condition (Compare 0). Remaining bits are
 * mirrored, e.g. samples for LSB of first byte are & 0xf
 *
 */

#include <sys/types.h>

#include "openpicc.h"
#include "dbgu.h"
#include "decoder.h"


#define OVERSAMPLING_RATE	4

/* definitions for four-times oversampling */
#define SEQ_X	0x4
#define SEQ_Y	0x0
#define SEQ_Z	0x1

/* decode a single sampled bit */
static u_int8_t miller_decode_sampled_bit(u_int32_t sampled_bit)
{
	switch (sampled_bit) {
	case SEQ_X:
		return 1;
		break;
	case SEQ_Z:
	case SEQ_Y:
		return 0;
		break;
	default:
		DEBUGP("unknown sequence sample `%x' ", sampled_bit);
		return 2;
		break;
	}
}

/* decode a single 32bit data sample of an 8bit miller encoded word */
static int miller_decode_sample(u_int32_t sample, u_int8_t *data)
{
	u_int8_t ret = 0;
	unsigned int i;

	for (i = 0; i < sizeof(sample)/OVERSAMPLING_RATE; i++) {
		u_int8_t bit = miller_decode_sampled_bit(sample & 0xf);

		if (bit == 1)
			ret |= 1;
		/* else do nothing since ret was initialized with 0 */

		/* skip shifting in case of last data bit */
		if (i == sizeof(sample)/OVERSAMPLING_RATE)
			break;

		sample = sample >> OVERSAMPLING_RATE;
		ret = ret << 1;
	}

	*data = ret;

	return ret;
}

static u_int32_t get_next_bytesample(struct decoder_state *ms,
				     u_int8_t *parity_sample)
{
	u_int32_t ret = 0;

	/* get remaining bits from the current word */
	ret = *(ms->buf32) >> ms->bit_ofs;
	/* move to next word */
	ms->buf32++;

	/* if required, get remaining bits from next word */
	if (ms->bit_ofs)
		ret |= *(ms->buf32) << (32 - ms->bit_ofs);
	
	*parity_sample = (*(ms->buf32) >> ms->bit_ofs & 0xf);

	/* increment bit offset (modulo 32) */
	ms->bit_ofs = (ms->bit_ofs + OVERSAMPLING_RATE) % 32;

	return ret;
}

struct decoder_algo miller_decoder = {
	.oversampling_rate = OVERSAMPLING_RATE,
	.bits_per_sampled_char = 9 * OVERSAMPLING_RATE,
	.bytesample_mask = 0xffffffff,
	.decode_sample = &miller_decode_sample,
	.get_next_bytesample = &get_next_bytesample,
};
