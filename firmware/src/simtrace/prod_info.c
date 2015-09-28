/* Maintainance of production information in SPI flash
 * (C) 2013 by Harald Welte <laforge@gnumonks.org>
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

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <AT91SAM7.h>
#include <lib_AT91SAM7.h>
#include <openpcd.h>

#include <os/dbgu.h>

#include "spi_flash.h"
#include "prod_info.h"

#include "../simtrace.h"

#define OTP_REGION_PRODINFO	1

#define PRODINFO_MAGIC	0x51075ACE

struct simtrace_prod_info
{
	/* magic value */
	uint32_t magic;
	/* unix timestamp of production date (0=unknown) */
	uint32_t production_ts;
	/* hardware version */
	uint32_t version;
	/* re-works applied */
	uint32_t reworks;
} __attribute__((packed));


int prod_info_write(uint32_t ts, uint32_t version, uint32_t reworks)
{
	struct simtrace_prod_info pi = {
		.magic = PRODINFO_MAGIC,
		.production_ts = ts,
		.version = version,
		.reworks = reworks,
	};
	uint8_t *pi8 = (uint8_t *) &pi;
	uint32_t addr = OTP_ADDR(OTP_REGION_PRODINFO);
	unsigned int rc;
	int i;

	spiflash_write_protect(0);

	for (i = 0; i < sizeof(pi); i++) {
		DEBUGPCR("0x%02x writing 0x%0x", addr+i, pi8[i]);
		spiflash_write_enable(1);
		rc = spiflash_otp_write(addr+i, pi8[i]);
		if (rc < 0)
			break;

		do { } while (spiflash_read_status() & 1);
	}

	spiflash_otp_set_lock(OTP_REGION_PRODINFO);

	spiflash_write_protect(1);

	return rc;
}

int prod_info_get(uint32_t *ver, uint32_t *reworks)
{
	struct simtrace_prod_info pi;
	uint32_t addr = OTP_ADDR(OTP_REGION_PRODINFO);

	spiflash_otp_read(addr, (uint8_t *) &pi, sizeof(pi));

	if (pi.magic != PRODINFO_MAGIC)
		return -1;

	if (ver)
		*ver = pi.version;

	if (reworks)
		*reworks = pi.reworks;

	return 0;
}
