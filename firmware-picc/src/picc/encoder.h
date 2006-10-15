#ifndef _ENCODER_H
#define _ENCODER_H

struct encoder_algo {
	u_int8_t oversampling_rate;
	u_int8_t samplebits_per_databit;
	int (*encode_frame)(char *sample_buf, int sample_buf_size,
			    const char *data_buf, int data_len);
};

struct encoder_state {
	struct encoder_algo *algo;
	u_int8_t *buf;
	u_int8_t *cur;
	u_int8_t bit_ofs;
	u_int8_t last_bit;
};

#endif /* _ENCODER_H */
