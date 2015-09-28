#ifndef _DECODER_H
#define _DECODER_H

struct decoder_state;

struct decoder_algo {
	uint8_t oversampling_rate;		
	uint8_t bits_per_sampled_char;
	uint32_t bytesample_mask;
	int (*decode_sample)(const uint32_t sample, uint8_t data);
	uint32_t (*get_next_bytesample)(struct decoder_state *st, uint8_t *parity_sample);
};

struct decoder_state {
	struct decoder_algo *algo;
	uint8_t bit_ofs;
	const char *buf;
	const uint32_t *buf32;
};

extern int decoder_register(int algnum, struct decoder_algo *algo);
extern int decoder_decode(uint8_t algo, const char *sample_buf,
		  	  int sample_buf_size, char *data_buf);

#define DECODER_MILLER		0
#define DECODER_NRZL		1
#define DECODER_NUM_ALGOS 	2

static struct decoder_algo nrzl_decoder;
static struct decoder_algo miller_decoder;

#endif
