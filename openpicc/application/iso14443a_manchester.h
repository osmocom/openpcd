#ifndef ISO14443A_MANCHESTER_H_
#define ISO14443A_MANCHESTER_H_

extern int manchester_encode(u_int8_t *sample_buf, u_int16_t sample_buf_len, const iso14443_frame *frame);
#endif /*ISO14443A_MANCHESTER_H_*/
