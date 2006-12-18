#ifndef _DECODER_H
#define _DECODER_H

struct decoder_state;

struct decoder_algo {
	u_int8_t oversampling_rate;		
	u_int8_t bits_per_sampled_char;
	u_int32_t bytesample_mask;
	int (*decode_sample)(const u_int32_t sample, u_int8_t data);
	u_int32_t (*get_next_bytesample)(struct decoder_state *st, u_int8_t *parity_sample);
};

struct decoder_state {
	struct decoder_algo *algo;
	u_int8_t bit_ofs;
	const char *buf;
	const u_int32_t *buf32;
};

extern int decoder_register(int algnum, struct decoder_algo *algo);
extern int decoder_decode(u_int8_t algo, const char *sample_buf,
		  	  int sample_buf_size, char *data_buf);

#define DECODER_MILLER		0
#define DECODER_NRZL		1
#define DECODER_NUM_ALGOS 	2

static struct decoder_algo nrzl_decoder;
static struct decoder_algo miller_decoder;

#endif
