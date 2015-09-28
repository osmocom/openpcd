/* Generic Philips CL RC632 Routines
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

#ifdef DEBUG
#undef DEBUG
#endif

#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <cl_rc632.h>
#include "rc632.h"
#include <os/dbgu.h>
#include <librfid/rfid_layer2_iso14443a.h>
#include <librfid/rfid_protocol_mifare_classic.h>

/* initially we use the same values as cm5121 */
#define OPENPCD_CW_CONDUCTANCE		0x3f
#define OPENPCD_MOD_CONDUCTANCE		0x3f
#define OPENPCD_14443A_BITPHASE		0xa9
#define OPENPCD_14443A_THRESHOLD	0xff
#define OPENPCD_14443B_BITPHASE		0xad
#define OPENPCD_14443B_THRESHOLD	0xff

#define RC632_TMO_AUTH1	14000

#define RC632_TIMEOUT_FUZZ_FACTOR	10

#define USE_IRQ

#define ENTER()		DEBUGPCRF("entering")
struct rfid_asic rc632;

static int 
rc632_set_bit_mask(struct rfid_asic_handle *handle, 
		   uint8_t reg, uint8_t mask, uint8_t val)
{
	int ret;
	uint8_t tmp;

	ret = opcd_rc632_reg_read(handle, reg, &tmp);
	if (ret < 0)
		return ret;

	/* if bits are already like we want them, abort */
	if ((tmp & mask) == val)
		return 0;

	return opcd_rc632_reg_write(handle, reg, (tmp & ~mask)|(val & mask));
}

int 
rc632_turn_on_rf(struct rfid_asic_handle *handle)
{
	ENTER();
	return opcd_rc632_set_bits(handle, RC632_REG_TX_CONTROL, 0x03);
}

int 
rc632_turn_off_rf(struct rfid_asic_handle *handle)
{
	ENTER();
	return opcd_rc632_clear_bits(handle, RC632_REG_TX_CONTROL, 0x03);
}

static int
rc632_power_up(struct rfid_asic_handle *handle)
{
	ENTER();
	return opcd_rc632_clear_bits(handle, RC632_REG_CONTROL, 
				RC632_CONTROL_POWERDOWN);
}

static int
rc632_power_down(struct rfid_asic_handle *handle)
{
	return opcd_rc632_set_bits(handle, RC632_REG_CONTROL,
			      RC632_CONTROL_POWERDOWN);
}

#define MAX_WRITE_LEN	16	/* see Sec. 18.6.1.2 of RC632 Spec Rev. 3.2. */

int
rc632_write_eeprom(struct rfid_asic_handle *handle, 
		   uint16_t addr, uint8_t len, uint8_t *data)
{
	uint8_t sndbuf[MAX_WRITE_LEN + 2];
	uint8_t reg;
	int ret;

	if (len > MAX_WRITE_LEN)
		return -EINVAL;
	if (addr < 0x10)
		return -EPERM;
	if (addr > 0x1ff)
		return -EINVAL;

	sndbuf[0] = addr & 0x00ff;	/* LSB */
	sndbuf[1] = addr >> 8;		/* MSB */
	memcpy(&sndbuf[2], data, len);

	ret = opcd_rc632_fifo_write(handle, len + 2, sndbuf, 0x03);
	if (ret < 0)
		return ret;

	ret = opcd_rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_WRITE_E2);
	if (ret < 0)
		return ret;
	
	ret = opcd_rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &reg);
	if (ret < 0)
		return ret;

	if (reg & RC632_ERR_FLAG_ACCESS_ERR)
		return -EPERM;

	while (1) {
		ret = opcd_rc632_reg_read(handle, RC632_REG_SECONDARY_STATUS, &reg);
		if (ret < 0)
			return ret;

		if (reg & RC632_SEC_ST_E2_READY) {
			/* the E2Write command must be terminated, See sec. 18.6.1.3 */
			ret = opcd_rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_IDLE);
			break;
		}
	}
	
	return ret;
}

int
rc632_read_eeprom(struct rfid_asic_handle *handle, uint16_t addr, uint8_t len,
		  uint8_t *recvbuf)
{
	uint8_t sndbuf[3];
	uint8_t err;
	int ret;

	sndbuf[0] = (addr & 0xff);
	sndbuf[1] = addr >> 8;
	sndbuf[2] = len;

	ret = opcd_rc632_fifo_write(handle, 3, sndbuf, 0x03);
	if (ret < 0)
		return ret;

	ret = opcd_rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_READ_E2);
	if (ret < 0)
		return ret;

	/* usleep(20000); */

	ret = opcd_rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &err);
	if (err & RC632_ERR_FLAG_ACCESS_ERR)
		return -EPERM;
	
	ret = opcd_rc632_reg_read(handle, RC632_REG_FIFO_LENGTH, &err);
	if (err < len)
		len = err;

	ret = opcd_rc632_fifo_read(handle, len, recvbuf);
	if (ret < 0)
		return ret;

	return len;
}

#define RC632_E2_PRODUCT_TYPE	0
#define RC632_E2_PRODUCT_SERIAL	8
#define RC632_E2_RS_MAX_P	14

int rc632_get_serial(struct rfid_asic_handle *handle,
		     uint32_t *serial)
{
	return rc632_read_eeprom(handle, RC632_E2_PRODUCT_SERIAL, 
				 4, (uint8_t *)serial);
}
