/* OOK / Mancherster encoder implementation for OpenPICC
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

/* This is for encoding according to ISO 14443-2 at 106kBps.
 *
 * We first do manchester encoding on the data, and then use it to
 * do on/off keying of the subcarrier.
 *
 * Since we also create the subcarrier in software (using a sample clock that
 * corresponds sixteen times the bit clokc), we need to alternate between
 * 01010101 patterns and 00000000 patterns to generate a signal that is
 * perceived as OOK of the subcarrier.
 */

#include <sys/types.h>

#include <os/dbgu.h>

#define BITP_HALFBIT_ON		0xAA
#define BITP_HALFBIT_OFF	0x00

/* 1:	modulation in first half of bit period
 * 0:	modulation in second half of bit period
 * SOF: modulation in first half of bit period
 * EOF: not modulate for full bit period
 */

static int encode_ookm(u_int8_t *sample_buf, int sample_buf_size, 
		       const u_int8_t *data_buf, int data_len)
{
	const u_int8_t *data_cur;

	/* we can't optimize this to use 32bit writes, since due to 
	 * the parity bit, we always produce 18 bytes output for one
	 * byte input */

	for (data_cur = data_buf;
	       data_cur < data_buf + data_len; data_cur++) {
		u_int8_t data = *data_cur;
		int parity = 0;
		int bit = 8;

		while (bit-- > 0) {
			if (data & 0x01) {
				*sample_buf++ = BITP_HALFBIT_ON;
				*sample_buf++ = BITP_HALFBIT_OFF;
				parity++;
			} else {
				*sample_buf++ = BITP_HALFBIT_OFF;
				*sample_buf++ = BITP_HALFBIT_ON;
			}
			data = data >> 1;
		}
		if (parity & 0x1 == 0) {
			*sample_buf++ = BITP_HALFBIT_ON;
			*sample_buf++ = BITP_HALFBIT_OFF;
		} else {
			*sample_buf++ = BITP_HALFBIT_ON;
			*sample_buf++ = BITP_HALFBIT_OFF;
		}
	}

	return 0;
}

static int encode_sof(u_int8_t *sample_buf)
{
	*sample_buf++ = BITP_HALFBIT_ON;
	*sample_buf++ = BITP_HALFBIT_OFF;

	return 2;
}

static int encode_eof(u_int8_t *sample_buf)
{
	*sample_buf++ = BITP_HALFBIT_OFF;
	*sample_buf++ = BITP_HALFBIT_OFF;

	return 2;
}


