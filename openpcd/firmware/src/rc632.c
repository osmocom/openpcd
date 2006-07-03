/* Philips CL RC632 driver (via SPI) 
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * */

#include "pio_irq.h"

static void spi_irq(void)
{
	u_int32_t status = pSPI->SPI_SR;

	if (status & (AT91C_SPI_OVRES|AT91C_SPI_MODF)) {
		/* FIXME: print error message to debug port */
	}
}

static u_int8_t spi_outbuf[64+1];
static u_int8_t spi_inbuf[64+1];

#define FIFO_ADDR (RC632_REG_FIFO_DATA << 1)

struct rc632 {
	u_int16_t flags;
	struct fifo fifo;
};

#define RC632_F_FIFO_TX		0x0001


/* RC632 access primitives */

void rc632_write_reg(u_int8_t addr, u_int8_t data)
{
	addr = (addr << 1) & 0x7e;
}

void rc632_write_fifo(u_int8_t len, u_int8_t *data)
{
	if (len > sizeof(spi_outbuf)-1)
		len = sizeof(spi_outbuf)-1;

	spi_outbuf[0] = FIFO_ADDR;
	memcpy(spi_outbuf[1], data, len);

	/* FIXME: transceive (len+1) */

	return len;
}

u_int8_t rc632_read_reg(u_int8_t addr)
{
	addr = (addr << 1) & 0x7e;
}

u_int8_t rc632_read_fifo(u_int8_t max_len, u_int8_t *data)
{
	u_int8_t fifo_length = rc632_reg_read(RC632_REG_FIFO_LENGTH);
	u_int8_t i;

	if (max_len < fifo_length)
		fifo_length = max_len;

	for (i = 0; i < fifo_length; i++)
		spi_outbuf[i] = FIFO_ADDR;

	/* FIXME: transceive */

	return fifo_length;
}

/* RC632 interrupt handling */


static void rc632_irq(void)
{
	/* CL RC632 has interrupted us */
	u_int8_t cause = rc632_read_reg(RC632_REG_INTERRUPT_RQ);

	/* ACK all interrupts */
	rc632_write_reg(RC632_REG_INTERRUPT_RQ, cause);

	if (cause & RC632_INT_LOALERT) {
		/* FIFO is getting low, refill from virtual FIFO */
		if (!fifo_available(fifo))
			break;
	}
	if (cause & RC632_INT_HIALERT) {
		/* FIFO is getting full, empty into virtual FIFO */
	}
	if (cause & RCR632_INT_TIMER) {
		/* Timer has expired, signal it to host */
	}
}

void rc632_init(void)
{
	AT91F_SPI_CfgPMC();
	AT91F_SPI_CfgPIO();	/* check whether we really need all this */
	AT91F_SPI_Enable();

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SPI, F,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &spi_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SPI);
	AT91F_SPI_EnableIt(pSPI, AT91C_SPI_MODF|AT91C_SPI_OVRES);
	AT91F_SPI_CfgMode(AT91C_SPI_MSTR|AT91C_SPI_PS_FIXED);
	/* CPOL = 0, NCPHA = 1, CSAAT = 0, BITS = 0000, SCBR = 10 (4.8MHz), 
	 * DLYBS = 0, DLYBCT = 0 */
	AT91F_SPI_CfgCs(pSPI, 0, AT91C_SPI_BITS8|AT91C_SPI_NCPHA|(10<<8));
	AT91F_SPI_Reset();

	/* Register rc632_irq */
	pio_irq_register(OPENPCD_RC632_IRQ, &rc632_irq);
	pio_irq_enable(OPENPCD_RC632_IRQ);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_RC632_RESET);
};

void rc632_exit(void)
{
	pio_irq_unregister(OPENPCD_RC632_IRQ);
	AT91F_AIC_DisableIt(AT91C_BASE_AIC, AT91C_ID_SPI);
	AT01F_SPI_Disable();
}
