/* Philips CL RC632 driver (via SPI) for OpenPCD firmware
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * This is heavily based on the librfid RC632 driver. All primitive access
 * functions such as rc632_{reg,fifo}_{read,write}() are API compatible to
 * librfid in order to be able to leverage higher-level code from librfid
 * to this OpenPCD firmware.
 *
 * AT91SAM7 PWM routines for OpenPCD / OpenPICC
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

#include <string.h>
#include <errno.h>
#include <lib_AT91SAM7.h>
#include <cl_rc632.h>
#include <openpcd.h>
#include "../openpcd.h"
#include <os/fifo.h>
#include <os/dbgu.h>
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include <os/req_ctx.h>
#include "rc632.h"

#define ALWAYS_RESPOND

#define NOTHING  do {} while(0)

#if 0
#define DEBUGPSPI DEBUGP
#define DEBUGPSPIIRQ DEBUGP
#else
#define	DEBUGPSPI(x, args ...)  NOTHING
#define DEBUGPSPIIRQ(x, args...) NOTHING
#endif

#if 0
#define DEBUG632 DEBUGPCRF
#else
#define DEBUG632(x, args ...)	NOTHING
#endif


/* SPI driver */

#ifdef OLIMEX
#define SPI_DEBUG_LOOPBACK
#endif

#define SPI_USES_DMA

#define SPI_MAX_XFER_LEN	65

static const AT91PS_SPI pSPI = AT91C_BASE_SPI;

/* SPI irq handler */
static void spi_irq(void)
{
	u_int32_t status = pSPI->SPI_SR;

	DEBUGPSPIIRQ("spi_irq: 0x%08x ", status);

	if (status & AT91C_SPI_OVRES)
		DEBUGPSPIIRQ("Overrun ");
	if (status & AT91C_SPI_MODF)
		DEBUGPSPIIRQ("ModeFault ");
	if (status & AT91C_SPI_ENDRX) {
		pSPI->SPI_IDR = AT91C_SPI_ENDRX;
		DEBUGPSPIIRQ("ENDRX ");
	}
	if (status & AT91C_SPI_ENDTX) {
		pSPI->SPI_IDR = AT91C_SPI_ENDTX;
		DEBUGPSPIIRQ("ENDTX ");
	}

	DEBUGPSPIIRQ("\r\n");

	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_SPI);
}

#ifdef SPI_USES_DMA
static int spi_transceive(const u_int8_t *tx_data, u_int16_t tx_len, 
			  u_int8_t *rx_data, u_int16_t *rx_len)
{
	DEBUGPSPI("DMA Xfer tx=%s\r\n", hexdump(tx_data, tx_len));
	if (*rx_len < tx_len) {
		DEBUGPCRF("rx_len=%u smaller tx_len=%u\n", *rx_len, tx_len);
		return -1;
	}

	AT91F_SPI_ReceiveFrame(pSPI, rx_data, tx_len, NULL, 0);
	AT91F_SPI_SendFrame(pSPI, tx_data, tx_len, NULL, 0);

	AT91F_PDC_EnableRx(AT91C_BASE_PDC_SPI);
	AT91F_PDC_EnableTx(AT91C_BASE_PDC_SPI);

	pSPI->SPI_IER = AT91C_SPI_ENDTX|AT91C_SPI_ENDRX;


	while (! (pSPI->SPI_SR & AT91C_SPI_ENDRX)) ;

	DEBUGPSPI("DMA Xfer finished rx=%s\r\n", hexdump(rx_data, tx_len));

	*rx_len = tx_len;

	return 0;
}
#else
/* stupid polling transceiver routine */
static int spi_transceive(const u_int8_t *tx_data, u_int16_t tx_len, 
		   u_int8_t *rx_data, u_int16_t *rx_len)
{
	u_int16_t tx_cur = 0;
	u_int16_t rx_len_max = 0;
	u_int16_t rx_cnt = 0;

	/* disable RC632 interrupt because it wants to do SPI transactions */
	AT91F_AIC_DisableIt(AT91C_BASE_AIC, OPENPCD_IRQ_RC632);

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
		DEBUGPSPI("leave(%02x %02x)\r\n", rx_data[0], rx_data[1]);
	else
		DEBUGPSPI("leave()\r\n");

	/* Re-enable RC632 interrupts */
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, OPENPCD_IRQ_RC632);

	return 0;
}
#endif

/* RC632 driver */

/* static buffers used by RC632 access primitives below. 
 * Since we only have one */

static u_int8_t spi_outbuf[SPI_MAX_XFER_LEN];
static u_int8_t spi_inbuf[SPI_MAX_XFER_LEN];

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

	DEBUG632("[0x%02x] <= 0x%02x", addr, data);

	addr = (addr << 1) & 0x7e;

	spi_outbuf[0] = addr;
	spi_outbuf[1] = data;

	//spi_transceive(spi_outbuf, 2, NULL, NULL);
	return spi_transceive(spi_outbuf, 2, spi_inbuf, &rx_len);
}

int rc632_fifo_write(struct rfid_asic_handle *hdl,
		     u_int8_t len, u_int8_t *data, u_int8_t flags)
{
	u_int16_t rx_len = sizeof(spi_inpuf);
	if (len > sizeof(spi_outbuf)-1)
		len = sizeof(spi_outbuf)-1;

	spi_outbuf[0] = FIFO_ADDR;
	memcpy(&spi_outbuf[1], data, len);

	DEBUG632("[FIFO] <= %s", hexdump(data, len));

	return spi_transceive(spi_outbuf, len+1, spi_inbuf, &rx_len);
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

	DEBUG632("[0x%02x] => 0x%02x", addr>>1, *val);

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

	DEBUG632("[FIFO] => %s", hexdump(data, rx_len-1));

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

static void rc632_irq(void)
{
	struct req_ctx *irq_rctx;
	struct openpcd_hdr *irq_opcdh;
	u_int8_t cause;

	/* CL RC632 has interrupted us */
	rc632_reg_read(RAH, RC632_REG_INTERRUPT_RQ, &cause);

	/* ACK all interrupts */
	//rc632_reg_write(RAH, RC632_REG_INTERRUPT_RQ, cause);
	rc632_reg_write(RAH, RC632_REG_INTERRUPT_RQ, RC632_INT_TIMER);
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
	

	irq_rctx = req_ctx_find_get(RCTX_STATE_FREE,
				    RCTX_STATE_RC632IRQ_BUSY);
	if (!irq_rctx) {
		DEBUGPCRF("NO RCTX!\n");
		/* disable rc632 interrupt until RCTX is free */
		AT91F_AIC_DisableIt(AT91C_BASE_AIC, OPENPCD_IRQ_RC632);
		return;
	}

	irq_opcdh = (struct openpcd_hdr *) &irq_rctx->tx.data[0];

	/* initialize static part of openpcd_hdr for USB IRQ reporting */
	irq_opcdh->cmd = OPENPCD_CMD_IRQ;
	irq_opcdh->flags = 0x00;
	irq_opcdh->reg = 0x07;
	irq_opcdh->val = cause;
	
	req_ctx_set_state(irq_rctx, RCTX_STATE_UDP_EP3_PENDING);
	DEBUGPCR("");
}

void rc632_unthrottle(void)
{
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, OPENPCD_IRQ_RC632);
}

void rc632_power(u_int8_t up)
{
        DEBUGPCRF("powering %s RC632", up ? "up" : "down");
	if (up)
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA,
				      OPENPCD_PIO_RC632_RESET);
	else
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA,
				    OPENPCD_PIO_RC632_RESET);
}

void rc632_reset(void)
{
	volatile int i;

	rc632_power(0);
	for (i = 0; i < 0xffff; i++)
		{}
	rc632_power(1);

	/* wait for startup phase to finish */
	while (1) {
		u_int8_t val;
		rc632_reg_read(RAH, RC632_REG_COMMAND, &val);
		if (val == 0x00)
			break;
	}

	/* turn off register paging */
	rc632_reg_write(RAH, RC632_REG_PAGE0, 0x00);
}

static int rc632_usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];
	u_int16_t len = rctx->rx.tot_len-sizeof(*poh);

	switch (poh->cmd) {
	case OPENPCD_CMD_READ_REG:
		rc632_reg_read(RAH, poh->reg, &pih->val);
		DEBUGP("READ REG(0x%02x)=0x%02x ", poh->reg, pih->val);
		goto respond;
		break;
	case OPENPCD_CMD_READ_FIFO:
		{
		u_int16_t req_len = poh->val, remain_len = req_len, pih_len;
		if (req_len > MAX_PAYLOAD_LEN) {
			pih_len = MAX_PAYLOAD_LEN;
			remain_len -= pih_len;
			rc632_fifo_read(RAH, pih_len, pih->data);
			rctx->tx.tot_len += pih_len;
			DEBUGP("READ FIFO(len=%u)=%s ", req_len,
				hexdump(pih->data, pih_len));
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
			udp_refill_ep(2, rctx);

			/* get and initialize second rctx */
			rctx = req_ctx_find_get(RCTX_STATE_FREE,
						RCTX_STATE_MAIN_PROCESSING);
			if (!rctx) {
				DEBUGPCRF("FATAL_NO_RCTX!!!\n");
				break;
			}
			poh = (struct openpcd_hdr *) &rctx->rx.data[0];
			pih = (struct openpcd_hdr *) &rctx->tx.data[0];
			memcpy(pih, poh, sizeof(*poh));
			rctx->tx.tot_len = sizeof(*poh);

			pih_len = remain_len;
			rc632_fifo_read(RAH, pih->val, pih->data);
			rctx->tx.tot_len += pih_len;
			DEBUGP("READ FIFO(len=%u)=%s ", pih_len,
				hexdump(pih->data, pih_len));
			/* don't set state of second rctx, main function
			 * body will do this after switch statement */
		} else {
			pih->val = poh->val;
			rc632_fifo_read(RAH, poh->val, pih->data);
			rctx->tx.tot_len += pih_len;
			DEBUGP("READ FIFO(len=%u)=%s ", poh->val,
				hexdump(pih->data, poh->val));
		}
		goto respond;
		break;
		}
	case OPENPCD_CMD_WRITE_REG:
		DEBUGP("WRITE_REG(0x%02x, 0x%02x) ", poh->reg, poh->val);
		rc632_reg_write(RAH, poh->reg, poh->val);
		break;
	case OPENPCD_CMD_WRITE_FIFO:
		DEBUGP("WRITE FIFO(len=%u): %s ", len,
			hexdump(poh->data, len));
		rc632_fifo_write(RAH, len, poh->data, 0);
		break;
	case OPENPCD_CMD_READ_VFIFO:
		DEBUGP("READ VFIFO ");
		DEBUGP("NOT IMPLEMENTED YET ");
		goto respond;
		break;
	case OPENPCD_CMD_WRITE_VFIFO:
		DEBUGP("WRITE VFIFO ");
		DEBUGP("NOT IMPLEMENTED YET ");
		break;
	case OPENPCD_CMD_REG_BITS_CLEAR:
		DEBUGP("CLEAR BITS ");
		pih->val = rc632_clear_bits(RAH, poh->reg, poh->val);
		break;
	case OPENPCD_CMD_REG_BITS_SET:
		DEBUGP("SET BITS ");
		pih->val = rc632_set_bits(RAH, poh->reg, poh->val);
		break;
	case OPENPCD_CMD_DUMP_REGS:
		DEBUGP("DUMP REGS ");
		DEBUGP("NOT IMPLEMENTED YET ");
		goto respond;
		break;
	default:
		DEBUGP("UNKNOWN ");
		return -EINVAL;
	}

#ifdef ALWAYS_RESPOND
	goto respond;
#endif

	req_ctx_put(rctx);
	DEBUGPCR("");
	return 0;

respond:
	req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
	/* FIXME: we could try to send this immediately */
	udp_refill_ep(2, rctx);
	DEBUGPCR("");

	return 1;
}

void rc632_init(void)
{
	//fifo_init(&rc632.fifo, 256, NULL, &rc632);

	DEBUGPCRF("entering");

	AT91F_SPI_CfgPMC();

	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA,
				AT91C_PA11_NPCS0|AT91C_PA12_MISO|
				AT91C_PA13_MOSI |AT91C_PA14_SPCK, 0);

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SPI,
			      OPENPCD_IRQ_PRIO_SPI,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &spi_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SPI);

	AT91F_SPI_EnableIt(pSPI, AT91C_SPI_MODF|AT91C_SPI_OVRES);
#ifdef SPI_USES_DMA
	AT91F_SPI_EnableIt(pSPI, AT91C_SPI_ENDRX|AT91C_SPI_ENDTX);
#endif

#ifdef SPI_DEBUG_LOOPBACK
	AT91F_SPI_CfgMode(pSPI, AT91C_SPI_MSTR|AT91C_SPI_PS_FIXED|
				AT91C_SPI_MODFDIS|AT91C_SPI_LLB);
#else
	AT91F_SPI_CfgMode(pSPI, AT91C_SPI_MSTR|AT91C_SPI_PS_FIXED|
				AT91C_SPI_MODFDIS);
#endif
	/* CPOL = 0, NCPHA = 1, CSAAT = 0, BITS = 0000, SCBR = 10 (4.8MHz), 
	 * DLYBS = 0, DLYBCT = 0 */
#ifdef SPI_USES_DMA
	AT91F_SPI_CfgCs(pSPI, 0, AT91C_SPI_BITS_8|AT91C_SPI_NCPHA|(10<<8));
#else
	/* 320 kHz in case of I/O based SPI */
	AT91F_SPI_CfgCs(pSPI, 0, AT91C_SPI_BITS_8|AT91C_SPI_NCPHA|(0x7f<<8));
#endif
	AT91F_SPI_Enable(pSPI);

	/* Register rc632_irq */
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, OPENPCD_IRQ_RC632,
			      OPENPCD_IRQ_PRIO_RC632,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &rc632_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, OPENPCD_IRQ_RC632);

	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_RC632_RESET);

	rc632_reset();

	/* configure IRQ pin */
	rc632_reg_write(RAH, RC632_REG_IRQ_PIN_CONFIG,
			RC632_IRQCFG_CMOS|RC632_IRQCFG_INV);
	/* enable interrupts */
	rc632_reg_write(RAH, RC632_REG_INTERRUPT_EN, RC632_INT_TIMER);
	
	/* configure AUX to test signal four */
	rc632_reg_write(RAH, RC632_REG_TEST_ANA_SELECT, 0x04);

	usb_hdlr_register(&rc632_usb_in, OPENPCD_CMD_CLS_RC632);
};

#if 0
void rc632_exit(void)
{
	usb_hdlr_unregister(OPENPCD_CMD_CLS_RC632);
	AT91F_AIC_DisableIt(AT91C_BASE_AIC, OPENPCD_IRQ_RC632);
	AT91F_AIC_DisableIt(AT91C_BASE_AIC, AT91C_ID_SPI);
	AT91F_SPI_Disable(pSPI);
}
#endif

#ifdef DEBUG
static int rc632_reg_write_verify(struct rfid_asic_handle *hdl,
				  u_int8_t reg, u_int8_t val)
{
	u_int8_t tmp;

	rc632_reg_write(hdl, reg, val);
	rc632_reg_read(hdl, reg, &tmp);

	DEBUGPCRF("reg=0x%02x, write=0x%02x, read=0x%02x ", reg, val, tmp);

	return (val == tmp);
}

int rc632_dump(void)
{
	u_int8_t i;
	u_int16_t rx_len = sizeof(spi_inbuf);

	for (i = 0; i <= 0x3f; i++) {
		u_int8_t reg = i;
		if (reg == RC632_REG_FIFO_DATA)
			reg = 0x3e;
			
		spi_outbuf[i] = reg << 1;
		spi_inbuf[i] = 0x00;
	}

	/* MSB of first byte of read spi transfer is high */
	spi_outbuf[0] |= 0x80;

	/* last byte of read spi transfer is 0x00 */
	spi_outbuf[0x40] = 0x00;
	spi_inbuf[0x40] = 0x00;

	spi_transceive(spi_outbuf, 0x41, spi_inbuf, &rx_len);

	for (i = 0; i < 0x3f; i++) {
		if (i == RC632_REG_FIFO_DATA)
			DEBUGPCR("REG 0x02 = NOT READ");
		else
			DEBUGPCR("REG 0x%02x = 0x%02x", i, spi_inbuf[i+1]);
	}
	
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
int rc632_test(struct rfid_asic_handle *hdl) {}
int rc632_dump(void) {}
#endif /* DEBUG */
