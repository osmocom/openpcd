/* Philips CL RC632 driver (via SPI) for OpenPCD firmware
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * This is heavily based on the librfid RC632 driver. All primitive access
 * functions such as rc632_{reg,fifo}_{read,write}() are API compatible to
 * librfid in order to be able to leverage higher-level code from librfid
 * to this OpenPCD firmware.
 *
 * */

#include <string.h>

#include <include/lib_AT91SAM7.h>
#include <include/cl_rc632.h>
#include <include/openpcd.h>
#include "openpcd.h"
#include "fifo.h"
#include "dbgu.h"
#include "pcd_enumerate.h"
#include "rc632.h"

/* SPI driver */

//#define SPI_DEBUG_LOOPBACK
#define SPI_USES_DMA

static AT91PS_SPI pSPI = AT91C_BASE_SPI;

/* SPI irq handler */
static void spi_irq(void)
{
	u_int32_t status = pSPI->SPI_SR;

	DEBUGP("spi_irq: 0x%08x ", status);

	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_SPI);

	if (status & AT91C_SPI_OVRES)
		DEBUGP("Overrun ");
	if (status & AT91C_SPI_MODF)
		DEBUGP("ModeFault ");
	if (status & AT91C_SPI_ENDRX) {
		pSPI->SPI_IDR = AT91C_SPI_ENDRX;
		DEBUGP("ENDRX ");
	}
	if (status & AT91C_SPI_ENDTX) {
		pSPI->SPI_IDR = AT91C_SPI_ENDTX;
		DEBUGP("ENDTX ");
	}

	DEBUGP("\r\n");
}

#ifdef SPI_USES_DMA
int spi_transceive(const u_int8_t *tx_data, u_int16_t tx_len, 
		   u_int8_t *rx_data, u_int16_t *rx_len)
{
	DEBUGP("Starting DMA Xfer: ");
	AT91F_SPI_ReceiveFrame(pSPI, rx_data, *rx_len, NULL, 0);
	AT91F_SPI_SendFrame(pSPI, tx_data, tx_len, NULL, 0);
	AT91F_PDC_EnableRx(AT91C_BASE_PDC_SPI);
	AT91F_PDC_EnableTx(AT91C_BASE_PDC_SPI);
	pSPI->SPI_IER = AT91C_SPI_ENDTX|AT91C_SPI_ENDRX;
	AT91F_SPI_Enable(pSPI);

	while (!(pSPI->SPI_SR & (AT91C_SPI_ENDRX|AT91C_SPI_ENDTX))) ;

	DEBUGPCR("DMA Xfer finished");

	return 0;
}
#else
/* stupid polling transceiver routine */
int spi_transceive(const u_int8_t *tx_data, u_int16_t tx_len, 
		   u_int8_t *rx_data, u_int16_t *rx_len)
{
	u_int16_t tx_cur = 0;
	u_int16_t rx_len_max = 0;
	u_int16_t rx_cnt = 0;

	/* disable RC632 interrupt because it wants to do SPI transactions */
	AT91F_AIC_DisableIt(AT91C_BASE_AIC, OPENPCD_RC632_IRQ);

	DEBUGPCRF("enter(tx_len=%u)", tx_len);

	if (rx_len) {
		rx_len_max = *rx_len;
		*rx_len = 0;
	}

	AT91F_SPI_Enable(pSPI);
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
	AT91F_SPI_Disable(pSPI);
	if (rx_data)
		DEBUGPCRF("leave(%02x %02x)", rx_data[0], rx_data[1]);
	else
		DEBUGPCRF("leave()");

	/* Re-enable RC632 interrupts */
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, OPENPCD_RC632_IRQ);

	return 0;
}
#endif

/* RC632 driver */

/* static buffers used by RC632 access primitives below. 
 * Since we only have one */

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

int rc632_reg_write(struct rfid_asic_handle *hdl,
		    u_int8_t addr, u_int8_t data)
{
	u_int16_t rx_len = 2;
	addr = (addr << 1) & 0x7e;

	spi_outbuf[0] = addr;
	spi_outbuf[1] = data;

	//spi_transceive(spi_outbuf, 2, NULL, NULL);
	return spi_transceive(spi_outbuf, 2, spi_inbuf, &rx_len);
}

int rc632_fifo_write(struct rfid_asic_handle *hdl,
		     u_int8_t len, u_int8_t *data, u_int8_t flags)
{
	if (len > sizeof(spi_outbuf)-1)
		len = sizeof(spi_outbuf)-1;

	spi_outbuf[0] = FIFO_ADDR;
	memcpy(&spi_outbuf[1], data, len);

	return spi_transceive(spi_outbuf, len+1, NULL, NULL);
}

int rc632_reg_read(struct rfid_asic_handle *hdl, 
		   u_int8_t addr, u_int8_t *val)
{
	u_int16_t rx_len = 2;

	addr = (addr << 1) & 0x7e;

	spi_outbuf[0] = addr | 0x80;
	spi_outbuf[1] = 0x00;

	spi_transceive(spi_outbuf, 2, spi_inbuf, &rx_len);

	*val = spi_inbuf[1];

	return 0;
}

int rc632_fifo_read(struct rfid_asic_handle *hdl,
		    u_int8_t max_len, u_int8_t *data)
{
	int ret;
	u_int8_t fifo_length;
	u_int8_t i;
	u_int16_t rx_len;

 	ret = rc632_reg_read(hdl, RC632_REG_FIFO_LENGTH, &fifo_length);
	if (ret < 0)
		return ret;

	rx_len = fifo_length+1;

	if (max_len < fifo_length)
		fifo_length = max_len;

	for (i = 0; i < fifo_length; i++)
		spi_outbuf[i] = FIFO_ADDR;

	spi_outbuf[0] |= 0x80;
	spi_outbuf[fifo_length] = 0x00;

	spi_transceive(spi_outbuf, fifo_length+1, spi_inbuf, &rx_len);
	memcpy(data, spi_inbuf+1, rx_len-1);

	return 0;
}

int rc632_set_bits(struct rfid_asic_handle *hdl,
		   u_int8_t reg, u_int8_t bits)
{
	u_int8_t val;
	int ret;
	
	ret = rc632_reg_read(hdl, reg, &val);
	if (ret < 0)
		return ret;

	val |= bits;

	return rc632_reg_write(hdl, reg, val);
}

int rc632_clear_bits(struct rfid_asic_handle *hdl,
		     u_int8_t reg, u_int8_t bits)
{
	u_int8_t val;
	int ret;
	
	ret = rc632_reg_read(hdl, reg, &val);
	if (ret < 0)
		return ret;

	val &= ~bits;

	return rc632_reg_write(hdl, reg, val);
}

/* RC632 interrupt handling */
static struct openpcd_hdr irq_opcdh;

static void rc632_irq(void)
{
	/* CL RC632 has interrupted us */
	u_int8_t cause;
	rc632_reg_read(RAH, RC632_REG_INTERRUPT_RQ, &cause);

	/* ACK all interrupts */
	rc632_reg_write(RAH, RC632_REG_INTERRUPT_RQ, cause);
	DEBUGP("rc632_irq: ");

	if (cause & RC632_INT_LOALERT) {
		/* FIFO is getting low, refill from virtual FIFO */
		DEBUGP("FIFO_low ");
		#if 0
		if (!fifo_available(&rc632.fifo))
			return;
		#endif
		/* FIXME */
	}
	if (cause & RC632_INT_HIALERT) {
		/* FIFO is getting full, empty into virtual FIFO */
		DEBUGP("FIFO_high ");
		/* FIXME */
	}
	/* All interrupts below can be reported directly to the host */
	if (cause & RC632_INT_TIMER)
		DEBUGP("Timer ");
	if (cause & RC632_INT_IDLE)
		DEBUGP("Idle ");
	if (cause & RC632_INT_RX)
		DEBUGP("RxComplete ");
	if (cause & RC632_INT_TX)
		DEBUGP("TxComplete ");
	
	irq_opcdh.val = cause;
	
	AT91F_UDP_Write(1, (u_int8_t *) &irq_opcdh, sizeof(irq_opcdh));
	DEBUGP("\n");
}

void rc632_power(u_int8_t up)
{
	if (up)
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA,
				      OPENPCD_PIO_RC632_RESET);
	else
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA,
				    OPENPCD_PIO_RC632_RESET);
}

void rc632_reset(void)
{
	int i;

	rc632_power(0);
	for (i = 0; i < 0xfffff; i++)
		{}
	rc632_power(1);

	/* turn off register paging */
	rc632_reg_write(RAH, RC632_REG_PAGE0, 0x00);
}

void rc632_init(void)
{
	//fifo_init(&rc632.fifo, 256, NULL, &rc632);

	DEBUGPCRF("entering");
	AT91F_SPI_CfgPMC();

	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA,
				AT91C_PA11_NPCS0|AT91C_PA12_MISO|
				AT91C_PA13_MOSI |AT91C_PA14_SPCK, 0);

	//AT91F_SPI_Enable(pSPI);

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SPI,
			      OPENPCD_IRQ_PRIO_SPI,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &spi_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SPI);

	AT91F_SPI_EnableIt(pSPI, AT91C_SPI_MODF|AT91C_SPI_OVRES);
#ifdef SPI_USES_DMA
	AT91F_SPI_EnableIt(pSPI, AT91C_SPI_ENDRX|AT91C_SPI_ENDTX);
#endif

#ifdef SPI_DEBUG_LOOPBACK
	AT91F_SPI_CfgMode(pSPI, AT91C_SPI_MSTR|AT91C_SPI_PS_FIXED|AT91C_SPI_LLB);
#else
	AT91F_SPI_CfgMode(pSPI, AT91C_SPI_MSTR|AT91C_SPI_PS_FIXED);
#endif
	/* CPOL = 0, NCPHA = 1, CSAAT = 0, BITS = 0000, SCBR = 10 (4.8MHz), 
	 * DLYBS = 0, DLYBCT = 0 */
	//AT91F_SPI_CfgCs(pSPI, 0, AT91C_SPI_BITS_8|AT91C_SPI_NCPHA|(10<<8));
	AT91F_SPI_CfgCs(pSPI, 0, AT91C_SPI_BITS_8|AT91C_SPI_NCPHA|(0x7f<<8));

	//AT91F_SPI_Reset(pSPI);

	/* Register rc632_irq */
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, OPENPCD_IRQ_RC632,
			      OPENPCD_IRQ_PRIO_RC632,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &rc632_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_IRQ1);

	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_RC632_RESET);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_MFIN);
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPCD_PIO_MFOUT);

	/* initialize static part of openpcd_hdr for USB IRQ reporting */
	irq_opcdh.cmd = OPENPCD_CMD_IRQ;
	irq_opcdh.flags = 0x00;
	irq_opcdh.reg = 0x07;
	irq_opcdh.len = 0x00;

	rc632_reset();
};

void rc632_exit(void)
{
	AT91F_AIC_DisableIt(AT91C_BASE_AIC, AT91C_ID_IRQ1);
	AT91F_AIC_DisableIt(AT91C_BASE_AIC, AT91C_ID_SPI);
	AT91F_SPI_Disable(pSPI);
}


#ifdef DEBUG
static int rc632_reg_write_verify(struct rfid_asic_handle *hdl,
				  u_int8_t reg, u_int8_t val)
{
	u_int8_t tmp;

	rc632_reg_write(hdl, reg, val);
	rc632_reg_read(hdl, reg, &tmp);

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
	
	return 0;
}

int rc632_test(struct rfid_asic_handle *hdl)
{
	if (rc632_reg_write_verify(hdl, RC632_REG_RX_WAIT, 0x55) != 1)
		return -1;

	if (rc632_reg_write_verify(hdl, RC632_REG_RX_WAIT, 0xAA) != 1)
		return -1;

	return 0;
}
#else /* DEBUG */
int rc632_test(void) {}
int rc632_dump(void) {}
#endif /* DEBUG */
