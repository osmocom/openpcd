/* BPSK encoder implementation for OpenPICC
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

/* This encoder is for encoding according to ISO 14443-2 at 212,424,838kHz
 *
 * Since the subcarrier is created entirely in software (using a sample clock
 * that corresponds to twice th subcarrier frequency), we need to alternate
 * between 01010101 and 10101010 patterns for BPSK.
 *
 */

/* The complexity of this encoder is significantly higher due to the
 * non-integral number of sample bytes per data bit, further 
 * accentuated by the non-intrgral-length SOF (which can e.g. be 33 subcarrier
 * clocks in length */

#include <sys/types.h>
#include <os/dbgu.h>

#define BITP_PHASE_0	0xAA
#define BITP_PHASE_1	0x55


static int append_one_bit(struct encoder_state *es,
				  int bit)
{
	int sbits = es->algo->samplebits_per_databit;
	u_int8_t new_sample;
	u_int8_t bits_left = 8 - es->bit_ofs;

	if (bit)
		new_sample = BIT_PHASE_1;
	else
		new_sample = BIT_PHASE_0;

	*es->cur |= ((new_sample & (1 << sbits-1)) << es->bit_ofs);
	if (es->bit_ofs + sbits >= 8) {
		es->bit_ofs = 0;
		es->cur++;
		if (bits_left < sbits) {
			/* we still have some bits to write */
			*es->cur = (new_sample >> bits_left);
			es->bit_ofs += sbits - bits_left;
			/* FIXME: what if we span three bytes of sample ? */
		}
	}

	return bits;
}

static int encode_bpsk(u_int8_t *sample_buf, int sample_buf_size,
		       const u_int8_t *data_buf, int data_len)
{
	const u_int8_t *data_cur;
	u_int32_t bits_added = 0;

	for (data_cur = data_buf;
	     data_cur < data_buf + data_len; data_cur++) {
		u_int8_t data = *data_cur;
		int parity = 0;
		int8_t last_bit = 0, bit = 8;

		while (bit-- > 0) {
			bits_added += append_one_bit(es, data);
			data = data >> 1;
		}
	}

	return bits_added;
}

static int encode_sof(struct encoder_state *es)
{
	/* first 32 subcarrier clocks  in 'phase == 1' */

	/* 16 clocks */
	*es->cur++ = BITP_PHASE_1;
	*es->cur++ = BITP_PHASE_1;
	*es->cur++ = BITP_PHASE_1;
	*es->cur++ = BITP_PHASE_1;

	/* 16 clocks */
	*es->cur++ = BITP_PHASE_1;
	*es->cur++ = BITP_PHASE_1;
	*es->cur++ = BITP_PHASE_1;
	*es->cur++ = BITP_PHASE_1;

	/* then one bit duration (dep. speed1) 'phase == 0' */
	return (8*8 + append_one_bit(es, 0));
}

static const struct encoder_algo enc14443b_106 = {
	.samplebits_per_databit = 16,
};

static const struct encoder_algo enc14443_212 = {
	.samplebits_per_databit = 8,
};

static const struct encoder_algo enc14443_424 = {
	.samplebits_per_databit = 4,
};

static const struct encoder_algo enc14443_848 = {
	.samplebits_per_databit = 2,
};
