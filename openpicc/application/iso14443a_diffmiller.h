#ifndef ISO14443A_DIFFMILLER_H_
#define ISO14443A_DIFFMILLER_H_

#include "iso14443.h"

struct diffmiller_state;

extern int iso14443a_decode_diffmiller(struct diffmiller_state *state, iso14443_frame *frame, 
	const u_int32_t buffer[], unsigned int *offset, const unsigned int buflen);
extern struct diffmiller_state *iso14443a_init_diffmiller(int pauses_count);

#endif /*ISO14443A_DIFFMILLER_H_*/
