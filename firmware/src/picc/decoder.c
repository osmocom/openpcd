/* Decoder Core for OpenPICC
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

#include <errno.h>
#include <sys/types.h>
#include <picc/decoder.h>

#include <os/dbgu.h>

static struct decoder_algo *decoder_algo[DECODER_NUM_ALGOS];

static int get_next_data(struct decoder_state *st, uint8_t *data)
{
	uint8_t parity_sample;
	uint32_t bytesample;

	bytesample = st->algo->get_next_bytesample(st, &parity_sample);

	return st->algo->decode_sample(bytesample, data);
}

/* iterate over sample buffer (size N bytes) and decode data */
int decoder_decode(uint8_t algo, const char *sample_buf,
	  	   int sample_buf_size, char *data_buf)
{
	int i, ret;
	struct decoder_state st;

	if (algo >= DECODER_NUM_ALGOS)
		return -EINVAL;

	st.buf = sample_buf;
	st.buf32 = (uint32_t *) st.buf;
	st.bit_ofs = 0;
	st.algo = decoder_algo[algo];

	for (i = 0; i < (sample_buf_size*8)/st.algo->bits_per_sampled_char;
	     i++) {
		ret = get_next_data(&st, &data_buf[i]);
		if (ret < 0) {
			DEBUGPCR("decoder error %d at data byte %u",
				 ret, i);
			return ret;
		}
	}

	return i+1;
}

int decoder_register(int algnum, struct decoder_algo *algo)
{
	if (algnum >= DECODER_NUM_ALGOS)
		return -EINVAL;

	decoder_algo[algnum] = algo;

	return 0;
}

void decoder_init(void)
{
	decoder_register(DECODER_MILLER, &miller_decoder);
	decoder_register(DECODER_NRZL, &nrzl_decoder);
}
