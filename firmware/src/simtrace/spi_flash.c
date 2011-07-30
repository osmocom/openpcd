/* Driver for a SST25VF040B spi flash attached to AT91SAM7 SPI
 * (C) 2011 by Harald Welte <hwelte@hmw-consulting.de>
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

#include <simtrace_usb.h>

#include <os/usb_handler.h>
#include <os/dbgu.h>
#include <os/pio_irq.h>

#include "../simtrace.h"
#include "../openpcd.h"

#define DEBUGPSPI DEBUGP
//#define DEBUGPSPI(x, y ...) do { } while (0)

static const AT91PS_SPI pSPI = AT91C_BASE_SPI;

void spiflash_write_protect(int on)
{
	if (on)
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, PIO_SPIF_nWP);
	else
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, PIO_SPIF_nWP);
}

#define SPI_PERIPHA (PIO_SPIF_SCK|PIO_SPIF_MOSI|PIO_SPIF_MISO|PIO_SPIF_nCS)

static __ramfunc void spi_irq(void)
{
	u_int32_t status = pSPI->SPI_SR;

	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_SPI);
}

void spiflash_init(void)
{
	DEBUGP("spiflash_init\r\n");

	/* activate and enable the write protection */
	AT91F_PIO_CfgPullupDis(AT91C_BASE_PIOA, PIO_SPIF_nWP);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, PIO_SPIF_nWP);
	spiflash_write_protect(1);

	/* Configure PIOs for SCK, MOSI, MISO and nCS */
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, SPI_PERIPHA, 0);

	AT91F_SPI_CfgPMC();
	/* Spansion flash in v1.0p only supprts Mode 3 or Mode 0 */
	/* Mode 3: CPOL=1 nCPHA=0 CSAAT=0 BITS=0(8) MCK/2 */
	AT91F_SPI_CfgCs(AT91C_BASE_SPI, 0, AT91C_SPI_CPOL |
					   AT91C_SPI_BITS_8 |
					   (64 << 8));

	/* SPI master mode, fixed CS, CS = 0 */
	AT91F_SPI_CfgMode(AT91C_BASE_SPI, AT91C_SPI_MSTR |
					  AT91C_SPI_PS_FIXED |
					  (0 << 16));

	/* configure interrupt controller for SPI IRQ */
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SPI,
			      OPENPCD_IRQ_PRIO_SPI,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &spi_irq);
	//AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SPI);

	/* Enable the SPI Controller */
	AT91F_SPI_Enable(AT91C_BASE_SPI);
	AT91F_SPI_EnableIt(AT91C_BASE_SPI, AT91C_SPI_MODF |
					   AT91C_SPI_OVRES |
					   AT91C_SPI_ENDRX |
					   AT91C_SPI_ENDTX);
}

static int spi_transceive(const u_int8_t *tx_data, u_int16_t tx_len, 
		   u_int8_t *rx_data, u_int16_t *rx_len)
{
	u_int16_t tx_cur = 0;
	u_int16_t rx_len_max = 0;
	u_int16_t rx_cnt = 0;

	DEBUGPSPI("spi_transceive: enter(tx_len=%u) ", tx_len);

	if (rx_len) {
		rx_len_max = *rx_len;
		*rx_len = 0;
	}

	//AT91F_SPI_Enable(pSPI);
	while (1) { 
		u_int32_t sr = pSPI->SPI_SR;
		u_int8_t tmp;
		if (sr & AT91C_SPI_RDRF) {
			tmp = pSPI->SPI_RDR;
			rx_cnt++;
			if (rx_len && *rx_len < rx_len_max)
				rx_data[(*rx_len)++] = tmp;
		}
	 	if (sr & AT91C_SPI_TDRE) {
			if (tx_len > tx_cur)
				pSPI->SPI_TDR = tx_data[tx_cur++];
		}
		if (tx_cur >= tx_len && rx_cnt >= tx_len)
			break;
	}
	//AT91F_SPI_Disable(pSPI);
	if (rx_data)
		DEBUGPSPI(" leave(%02x %02x)\r\n", rx_data[0], rx_data[1]);
	else
		DEBUGPSPI("leave()\r\n");

	return 0;
}

void spiflash_id(void)
{
	const u_int8_t tx_data[] = { 0x9f, 0, 0, 0 };
	u_int8_t rx_data[] = { 0,0,0,0 };
	u_int16_t rx_len = sizeof(rx_data);

 	spi_transceive(tx_data, sizeof(tx_data), rx_data, &rx_len);
	DEBUGP("SPI ID: %02x %02x %02x\n", rx_data[1], rx_data[2], rx_data[3]);
}
