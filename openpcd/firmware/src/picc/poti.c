/* SPI Potentiometer AD 7367 Driver for OpenPICC
 * (C) by Harald Welte <hwelte@hmw-consulting.de>
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
	for (i = 0; i < 0xff; i++) { }
	/* shift one left, since it is a seven-bit value written as 8 bit xfer */
	spi->SPI_TDR = position;
	while (!(spi->SPI_SR & AT91C_SPI_TDRE)) { }
	for (i = 0; i < 0xff; i++) { }
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SS2_DT_THRESH);
}

void poti_init(void)
{
	AT91F_SPI_CfgPMC();
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, AT91C_PA12_MISO | 
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
	/* CPOL = 0, NCPHA = 1, CSAAT = 0, BITS = 0000, SCBR = 12 (4MHz),
	 * DLYBS = 0, DLYBCT = 0 */
	AT91F_SPI_CfgCs(spi, 0, AT91C_SPI_BITS_8 | AT91C_SPI_NCPHA | (24<<8));
	AT91F_SPI_Enable(spi);
}
