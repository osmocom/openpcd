/* ISO 14443 B Rx (PCD->PICC) implementation
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * speed(kbps)	106	212	424	848
 * etu		128/fc	64/fc	32/fc	16/fc
 * etu(usec)	9.4	4.7	2.35	1.18
 *
 * NRZ-L coding with logic level
 *
 * logic 1:	carrier high field amplitude (no modulation)
 * logic 0:	carrier low field amplitude
 *
 * Character transmission format:
 *	start bit:	logic 0
 *	data:		eight bits, lsb first
 *	stop bit	logic 1
 *
 * Frame Format:
 *
 * 		SOF char [EGT char, ...] EOF
 *
 * 	SOF: falling edge, 10..11 etu '0', rising edge in 1etu, 2..3etu '1'
 *	EGT: between 0 and 57uS 
 *	EOF: falling edge, 10..11 etu '0', rising edge in 1etu
 *
 *
 * Sampling
 * - sample once per bit clock, exactly in the middle of it
 * - synchronize CARRIER_DIV TC0 to first falling edge
 * - Configure CARRIER_DIV RA compare (rising edge) to be at
 *   etu/2 carrier clocks.
 * - problem: SOF 12..14etu length, therefore we cannot specify
 *            SOF as full start condition and then sample with 10bit
 *            frames :(
 * 
 */

#include <errno.h>
#include <sys/types.h>
#include <os/dbgu.h>
#include <picc/decoder.h>

/* currently this code will only work with oversampling_rate == 1 */
#define OVERSAMPLING_RATE 1

static u_int32_t get_next_bytesample(struct decoder_state *st,
				     u_int8_t *parity_sample)
{
	u_int32_t ret = 0;
	u_int8_t bits_per_sampled_char = st->algo->bits_per_sampled_char;
	u_int8_t bytesample_mask = st->algo->bytesample_mask;

	/* FIXME: shift start and stop bit into parity_sample and just
	 * return plain 8-bit data word */
	
	/* first part of 10-databit bytesample */
	ret = (*(st->buf32) >> st->bit_ofs) & bytesample_mask;

	if (st->bit_ofs > 32 - bits_per_sampled_char) {
		/* second half of 10-databit bytesample */
		st->buf32++;
		ret |= (*(st->buf32) << (32 - st->bit_ofs));
	}
	st->bit_ofs = (st->bit_ofs + bits_per_sampled_char) % 32;

	return ret & bytesample_mask;
}

static int nrzl_decode_sample(const u_int32_t sample, u_int8_t *data)
{
	*data = (sample >> 1) & 0xff;

	if (!(sample & 0x01)) {
		DEBUGPCRF("invalid start bit 0!");
		return -EIO;
	}
	if (sample & 0x20) {
		DEBUGPCRF("invalid stop bit 1!");
		return -EIO;
	}

	return 0;
}

static struct decoder_algo nrzl_decoder = {
	.oversampling_rate = OVERSAMPLING_RATE,
	.bits_per_sampled_char = 10 * OVERSAMPLING_RATE,
	.bytesample_mask = 0x3ff,
	.decode_sample = &nrzl_decode_sample,
	.get_next_bytesample = &get_next_bytesample,
};
