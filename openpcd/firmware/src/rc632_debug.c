
#include "rc632.h"
#include "dbgu.h"
#include <include/cl_rc632.h>

#ifdef DEBUG
static int rc632_reg_write_verify(u_int8_t reg, u_int8_t val)
{
	u_int8_t tmp;

	rc632_reg_write(reg, val);
	tmp = rc632_reg_read(reg);

	DEBUGP("reg=0x%02x, write=0x%02x, read=0x%02x ", reg, val, tmp);

	return (val == tmp);
}

static u_int8_t tx_buf[0x40+1];
static u_int8_t rx_buf[0x40+1];


int rc632_dump(void)
{
	u_int8_t i;
	u_int16_t rx_len = sizeof(rx_buf);

	for (i = 0; i <= 0x3f; i++) {
		tx_buf[i] = i << 1;
		rx_buf[i] = 0x00;
	}

	/* MSB of first byte of read spi transfer is high */
	tx_buf[0] |= 0x80;

	/* last byte of read spi transfer is 0x00 */
	tx_buf[0x40] = 0x00;
	rx_buf[0x40] = 0x00;

	spi_transceive(tx_buf, 0x41, rx_buf, &rx_len);

	for (i = 0; i < 0x3f; i++)
		DEBUGP("REG 0x%02x = 0x%02x\r\n", i, rx_buf[i+1]);
}

int rc632_test(void)
{
	if (rc632_reg_write_verify(RC632_REG_RX_WAIT, 0x55) != 1)
		return -1;

	if (rc632_reg_write_verify(RC632_REG_RX_WAIT, 0xAA) != 1)
		return -1;

	return 0;
}
#else
int rc632_test(void)
{}
#endif
