/* SPI DAC AD5300 Driver for OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 * All bugs added by Henryk Plötz <henryk@ploetzli.ch> (c) 2006-2007
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


#include "openpicc.h"
#include "board.h"

#include <sys/types.h>
#include <lib_AT91SAM7.h>

static const AT91PS_SPI spi = AT91C_BASE_SPI;
static u_int8_t last_value;

void da_comp_carr(u_int8_t position)
{
	volatile int i;

	while (!(spi->SPI_SR & AT91C_SPI_TDRE)) { }
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SS2_DT_THRESH);
	/* shift four left, since it is an eight-bit value written as 16 bit xfer 
	   bits 15 and 14 are don't care
	   bits 13 and 12 are mode (0 is normal operation)
	   bits 11 thru 4 are data
	   bits 3 thru 0 are don't care
	 */
	spi->SPI_TDR = position << 4;
	while (!(spi->SPI_SR & AT91C_SPI_TDRE)) { }
	for (i = 0; i < 0xff; i++) { }
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SS2_DT_THRESH);
	last_value = position;
}

u_int8_t da_get_value(void)
{
	return last_value;
}

void da_init(void)
{
	AT91F_SPI_CfgPMC();
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 
			    AT91C_PA13_MOSI | AT91C_PA14_SPCK, 0);

	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SS2_DT_THRESH);
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SS2_DT_THRESH);

#if 0
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SPI,
			      OPENPCD_IRQ_PRIO_SPI,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &spi_irq);
	AT91G_AIC_EnableIt(AT9C_BASE_AIC, AT91C_ID_SPI);
#endif
	AT91F_SPI_CfgMode(spi, AT91C_SPI_MSTR | 
			  AT91C_SPI_PS_FIXED | AT91C_SPI_MODFDIS);
	/* CPOL = 1, NCPHA = 1, CSAAT = 0, BITS = 1000, SCBR = 13 (3.69MHz),
	 * DLYBS = 6 (125nS), DLYBCT = 0 */
	AT91F_SPI_CfgCs(spi, 0, AT91C_SPI_CPOL | AT91C_SPI_BITS_16 | 
			AT91C_SPI_NCPHA | (13 << 8) | (6 << 16));
	AT91F_SPI_Enable(spi);
	
	da_comp_carr(DA_BASELINE);
}
