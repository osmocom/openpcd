/* Decoder Core for OpenPCD / OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 */

#include <sys/types.h>
#include "decoder.h"

static struct decoder_algo *decoder_algo[DECODER_NUM_ALGOS];

static int get_next_data(struct decoder_state *st, u_int8_t *data)
{
	u_int8_t parity_sample;
	u_int32_t bytesample;

	bytesample = st->algo->get_next_bytesample(st, &parity_sample);

	return st->algo->decode_sample(bytesample, data);
}

/* iterate over sample buffer (size N bytes) and decode data */
int decoder_decode(u_int8_t algo, const char *sample_buf,
	  	   int sample_buf_size, char *data_buf)
{
	int ret;
	struct decoder_state st;

	if (algo >= DECODER_NUM_ALGOS)
		return -EINVAL;

	st.buf = sample_buf;
	st.buf32 = (u_int32_t *) st.buf;
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

int decoder_register(int algo, struct decoder_algo *algo)
{
	if (algo >= DECODER_NUM_ALGOS)
		return -EINVAL;

	decoder_algos[algo] = algo;

	return 0;
}

int decoder_init(void)
{
	decoder_register(DECODER_MILLER, &miller_decoder);
	decoder_register(DECODER_NRZL, &nrzl_decoder);
}
