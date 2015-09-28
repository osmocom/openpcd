/* ISO14443A Manchester encoder for OpenPICC
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
 * Definitions for 106kBps, at sampling clock 1695kHz
 *
 * 		bit sample pattern for one bit cycle
 * 		MSB first		LSB first	hex LSB first
 * Sequence D	1010101000000000	0000000001010101	0x0055
 * Sequence E	0000000010101010	0101010100000000	0x5500
 * Sequence F	1010101010101010	0101010101010101	0x5555
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
#define MANCHESTER_SEQ_F	0x5555

static uint32_t manchester_sample_size(uint8_t frame_bytelen)
{
	/* 16 bits (2 bytes) per bit => 16 bytes samples per data byte,
	 * plus 16bit (2 bytes) parity per data byte
	 * plus 16bit (2 bytes) SOF plus 16bit (2 bytes) EOF */
	return (frame_bytelen*18) + 2 + 2;

	/* this results in a maximum samples-per-frame size of 4612 bytes
	 * for a 256byte frame */
}

struct manch_enc_state {
	const char *data;
	char *samples;
	uint16_t *samples16;
};

static void manchester_enc_byte(struct manch_enc_state *mencs, uint8_t data)
{
	int i;
	uint8_t sum_1 = 0;

	/* append 8 sample blobs, one for each bit */
	for (i = 0; i < 8; i++) {
		if (data & (1 << i)) {
			*(mencs->samples16) = MANCHESTER_SEQ_D;
			sum_1++;
		} else {
			*(mencs->samples16) = MANCHESTER_SEQ_E;
		}
		mencs->samples16++
	}
	/* append odd parity */
	if (sum_1 & 0x01)
		*(mencs->samples16) = MANCHESTER_SEQ_E;
	else
		*(mencs->samples16) = MANCHESTER_SEQ_D;
	mencs->samples16++
}
 
int manchester_encode(char *sample_buf, uint16_t sample_buf_len, 
		      const char *data, uint8_t data_len)
{
	int i, enc_size;
	struct manch_enc_state mencs

	enc_size = manchester_sample_size(data_len);

	if (sample_buf_len < enc_size)
		return -EINVAL;

	/* SOF */
	*(mencs.samples16++) = MANCHESTER_SEQ_D;

	for (i = 0; i < data_len; i++)
		manchester_enc_byte(mencs, data[i]);
		
	/* EOF */
	*(mencs.samples16++) = MANCHESTER_SEQ_F;

	return enc_size;
}

#define BPSK_SPEED_212	


static uint32_t bpsk_sample_size(uint8_t frame_bytelen)

int bpsk_encode(char *sample_buf, uint16_t sample_buf_len,
		const char *data, uint8_t data_len)
{
	/* burst of 32 sub carrier cycles */	
	memset(sample_buf, 0x55, 8);

}
