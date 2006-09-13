/* SPI Potentiometer AD 7367 Driver for OpenPICC
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



#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include "../openpcd.h"

static const AT91PS_SPI spi = AT91C_BASE_SPI;

void poti_comp_carr(u_int8_t position)
{
	volatile int i;

	while (!(spi->SPI_SR & AT91C_SPI_TDRE)) { }
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SS2_DT_THRESH);
	//for (i = 0; i < 0xff; i++) { }
	/* shift one left, since it is a seven-bit value written as 8 bit xfer */
	spi->SPI_TDR = position & 0x7f;
	while (!(spi->SPI_SR & AT91C_SPI_TDRE)) { }
	for (i = 0; i < 0xff; i++) { }
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SS2_DT_THRESH);
}

void poti_reset(void)
{
	volatile int i;
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_nSLAVE_RESET);
	for (i = 0; i < 0xff; i++) { }
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_nSLAVE_RESET);
}

void poti_init(void)
{
	AT91F_SPI_CfgPMC();
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 
			    AT91C_PA13_MOSI | AT91C_PA14_SPCK, 0);

	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SS2_DT_THRESH);
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SS2_DT_THRESH);

	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_nSLAVE_RESET);
	poti_reset();

#if 0
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SPI,
			      OPENPCD_IRQ_PRIO_SPI,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &spi_irq);
	AT91G_AIC_EnableIt(AT9C_BASE_AIC, AT91C_ID_SPI);
#endif
	AT91F_SPI_CfgMode(spi, AT91C_SPI_MSTR | 
			  AT91C_SPI_PS_FIXED | AT91C_SPI_MODFDIS);
	/* CPOL = 0, NCPHA = 1, CSAAT = 0, BITS = 0000, SCBR = 13 (3.69MHz),
	 * DLYBS = 6 (125nS), DLYBCT = 0 */
	AT91F_SPI_CfgCs(spi, 0, AT91C_SPI_BITS_8 | AT91C_SPI_NCPHA |
			(13 << 8) | (6 << 16));
	AT91F_SPI_Enable(spi);
}
