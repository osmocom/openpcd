/* Philips CL RC632 driver (via SPI) 
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * */

#include <string.h>

#include <include/lib_AT91SAM7S64.h>
#include <include/cl_rc632.h>
#include "openpcd.h"
#include "fifo.h"
#include "dbgu.h"

static AT91PS_SPI pSPI = AT91C_BASE_SPI;

/* SPI irq handler */
static void spi_irq(void)
{
	u_int32_t status = pSPI->SPI_SR;

	DEBUGP("spi_irq: 0x%08x", status);

	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_SPI);

	if (status & AT91C_SPI_OVRES)
		DEBUGP("Overrun detected ");
	if (status & AT91C_SPI_MODF)
		DEBUGP("Mode Fault detected ");

	DEBUGP("\r\n");
}

/* stupid polling transceiver routine */
static int spi_transceive(const u_int8_t *tx_data, u_int16_t tx_len, 
			   u_int8_t *rx_data, u_int16_t *rx_len)
{
	u_int16_t tx_cur = 0;
	u_int16_t rx_len_max = 0;
	if (rx_len) {
		rx_len_max = *rx_len;
		*rx_len = 0;
	}

	AT91F_SPI_Enable(pSPI);
	while (1) { 
		u_int32_t sr = pSPI->SPI_SR;
		DEBUGP("SPI_SR 0x%08x\r\n", sr);
	 	if (sr & AT91C_SPI_TDRE) {
			DEBUGP("TDRE, sending byte\r\n");
			pSPI->SPI_TDR = tx_data[tx_cur++];
		}
		if (rx_len && *rx_len < rx_len_max) {
			if (sr & AT91C_SPI_RDRF) {
				DEBUGP("RDRF, reading byte\r\n");
				rx_data[(*rx_len)++] = pSPI->SPI_RDR;
			}
		}
		if (tx_cur >= tx_len) {
			if (!rx_len)
				return 0;

			if (*rx_len >= tx_len) 
				return 0;
		}

	}
	return 0;
}

/* static buffers used by routines below */
static u_int8_t spi_outbuf[64+1];
static u_int8_t spi_inbuf[64+1];

#define FIFO_ADDR (RC632_REG_FIFO_DATA << 1)

struct rc632 {
	u_int16_t flags;
	struct fifo fifo;
};
#define RC632_F_FIFO_TX		0x0001
static struct rc632 rc632;

/* RC632 access primitives */

void rc632_reg_write(u_int8_t addr, u_int8_t data)
{
	addr = (addr << 1) & 0x7e;

	spi_outbuf[0] = addr;
	spi_outbuf[1] = data;

	/* transceive */
	spi_transceive(spi_outbuf, 2, NULL, NULL);
}

int rc632_fifo_write(u_int8_t len, u_int8_t *data)
{
	if (len > sizeof(spi_outbuf)-1)
		len = sizeof(spi_outbuf)-1;

	spi_outbuf[0] = FIFO_ADDR;
	memcpy(&spi_outbuf[1], data, len);

	/* transceive (len+1) */
	spi_transceive(spi_outbuf, len+1, NULL, NULL);

	return len;
}

u_int8_t rc632_reg_read(u_int8_t addr)
{
	u_int16_t rx_len = 2;

	addr = (addr << 1) & 0x7e;

	spi_outbuf[0] = addr | 0x01;
	spi_outbuf[1] = 0x00;

	/* transceive */
	spi_transceive(spi_outbuf, 2, spi_inbuf, &rx_len);

	return spi_inbuf[1];
}

u_int8_t rc632_fifo_read(u_int8_t max_len, u_int8_t *data)
{
	u_int8_t fifo_length = rc632_reg_read(RC632_REG_FIFO_LENGTH);
	u_int8_t i;
	u_int16_t rx_len = fifo_length+1;

	if (max_len < fifo_length)
		fifo_length = max_len;

	for (i = 0; i < fifo_length; i++)
		spi_outbuf[i] = FIFO_ADDR;

	spi_outbuf[0] |= 0x01;
	spi_outbuf[fifo_length] = 0x00;

	/* transceive */
	spi_transceive(spi_outbuf, fifo_length+1, spi_inbuf, &rx_len);
	memcpy(data, spi_inbuf+1, rx_len-1);

	return rx_len-1;
}

/* RC632 interrupt handling */


static void rc632_irq(void)
{
	/* CL RC632 has interrupted us */
	u_int8_t cause = rc632_reg_read(RC632_REG_INTERRUPT_RQ);

	/* ACK all interrupts */
	rc632_reg_write(RC632_REG_INTERRUPT_RQ, cause);
	DEBUGP("rc632_irq: ");

	if (cause & RC632_INT_LOALERT) {
		/* FIFO is getting low, refill from virtual FIFO */
		DEBUGP("FIFO low alert ");
		#if 0
		if (!fifo_available(&rc632.fifo))
			return;
		#endif
		/* FIXME */
	}
	if (cause & RC632_INT_HIALERT) {
		/* FIFO is getting full, empty into virtual FIFO */
		DEBUGP("FIFO high alert ");
		/* FIXME */
	}
	if (cause & RC632_INT_TIMER) {
		/* Timer has expired, signal it to host */
		DEBUGP("Timer alert ");
		/* FIXME */
	}
	DEBUGP("\n");
}

void rc632_power(u_int8_t up)
{
	if (up)
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_RC632_RESET);
	else
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_RC632_RESET);
}

void rc632_reset(void)
{
	rc632_power(0);
	rc632_power(1);
}

void rc632_init(void)
{
	//fifo_init(&rc632.fifo, 256, NULL, &rc632);

	DEBUGP("rc632_pmc\r\n");
	AT91F_SPI_CfgPMC();

	DEBUGP("rc632_pio\r\n");
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA,
				AT91C_PA11_NPCS0|AT91C_PA12_MISO|
				AT91C_PA13_MOSI |AT91C_PA14_SPCK, 0);

	DEBUGP("rc632_en_spi\r\n");
	//AT91F_SPI_Enable(pSPI);

	DEBUGP("rc632_cfg_it_spi\r\n");
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SPI, AT91C_AIC_PRIOR_LOWEST,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &spi_irq);
	DEBUGP("rc632_en_it_spi\r\n");
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SPI);

	DEBUGP("rc632_spi_en_it\r\n");
	//AT91F_SPI_EnableIt(pSPI, AT91C_SPI_MODF|AT91C_SPI_OVRES|AT91C_SPI_RDRF|AT91C_SPI_TDRE);
	DEBUGP("rc632_spi_cfg_mode\r\n");
	AT91F_SPI_CfgMode(pSPI, AT91C_SPI_MSTR|AT91C_SPI_PS_FIXED);
	/* CPOL = 0, NCPHA = 1, CSAAT = 0, BITS = 0000, SCBR = 10 (4.8MHz), 
	 * DLYBS = 0, DLYBCT = 0 */
	DEBUGP("rc632_spi_cfg_cs\r\n");
	AT91F_SPI_CfgCs(pSPI, 0, AT91C_SPI_BITS_8|AT91C_SPI_NCPHA|(10<<8));

	//DEBUGP("rc632_spi_reset\r\n");
	//AT91F_SPI_Reset(pSPI);

	/* Register rc632_irq */
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_IRQ1, AT91C_AIC_PRIOR_LOWEST,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &rc632_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_IRQ1);

	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_RC632_RESET);

	DEBUGP("rc632_reset\r\n");
	rc632_reset();
};

void rc632_exit(void)
{
	AT91F_AIC_DisableIt(AT91C_BASE_AIC, AT91C_ID_IRQ1);
	AT91F_AIC_DisableIt(AT91C_BASE_AIC, AT91C_ID_SPI);
	AT91F_SPI_Disable(pSPI);
}
