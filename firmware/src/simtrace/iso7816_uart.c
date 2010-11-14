/* Driver for AT91SAM7 USART0 in ISO7816-3 mode
 * (C) 2010 by Harald Welte <hwelte@hmw-consulting.de>
 */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <AT91SAM7.h>
#include <lib_AT91SAM7.h>
#include <openpcd.h>

#include <os/usb_handler.h>
#include <os/dbgu.h>
#include <os/pio_irq.h>

#include "../simtrace.h"
#include "../openpcd.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const AT91PS_USART usart = AT91C_BASE_US0;

enum iso7816_3_state {
	ISO7816_S_RESET,	/* in Reset */
	ISO7816_S_WAIT_ATR,	/* waiting for ATR to start */
	ISO7816_S_IN_ATR,
	ISO7816_S_WAIT_READER,	/* waiting for data from reader */
	ISO7816_S_WAIT_CARD,	/* waiting for data from card */
};

enum atr_state {
	ATR_S_WAIT_TS,
	ATR_S_WAIT_T0,
	ATR_S_WAIT_TA,
	ATR_S_WAIT_TB,
	ATR_S_WAIT_TC,
	ATR_S_WAIT_TD,
	ATR_S_WAIT_HIST,
	ATR_S_WAIT_TCK,
	ATR_S_DONE,
};

struct iso7816_3_handle {
	enum iso7816_3_state state;

	u_int8_t fi;
	u_int8_t di;

	u_int8_t atr_idx;
	u_int8_t atr_hist_len;
	u_int8_t atr_last_td;
	enum atr_state atr_state;
	u_int8_t atr[64];

	u_int16_t apdu_len;
	u_int16_t apdu_idx;
};

struct iso7816_3_handle isoh;


/* Table 6 from ISO 7816-3 */
static const u_int16_t fi_table[] = {
	0, 372, 558, 744, 1116, 1488, 1860, 0,
	0, 512, 768, 1024, 1536, 2048, 0, 0
};

/* Table 7 from ISO 7816-3 */
static const u_int8_t di_table[] = {
	0, 1, 2, 4, 8, 16, 0, 0,
	0, 0, 2, 4, 8, 16, 32, 64,
};

static int compute_fidi_ratio(u_int8_t fi, u_int8_t di)
{
	u_int16_t f, d;
	int ret;

	if (fi >= ARRAY_SIZE(fi_table) ||
	    di >= ARRAY_SIZE(di_table))
		return -EINVAL;

	f = fi_table[fi];
	if (f == 0)
		return -EINVAL;

	d = di_table[di];
	if (d == 0)
		return -EINVAL;

	if (di < 8) 
		ret = f / d;
	else
		ret = f * d;

	return ret;
}

static void set_atr_state(struct iso7816_3_handle *ih, enum atr_state new_atrs)
{
	if (new_atrs == ATR_S_WAIT_TS) {
		ih->atr_idx = 0;
		ih->atr_hist_len = 0;
		ih->atr_last_td = 0;
		memset(ih->atr, 0, sizeof(ih->atr));
	} else if (ih->atr_state == new_atrs)
		return;

	//DEBUGPCR("ATR state %u -> %u", ih->atr_state, new_atrs);
	ih->atr_state = new_atrs;
}

static void set_state(struct iso7816_3_handle *ih, enum iso7816_3_state new_state)
{
	if (new_state == ISO7816_S_RESET) {
		usart->US_CR |= AT91C_US_RXDIS | AT91C_US_RSTRX;
	} else if (new_state == ISO7816_S_WAIT_ATR) {
		int rc;
		/* Initial Fi / Di ratio */
		ih->fi = 1;
		ih->di = 1;
		rc = compute_fidi_ratio(ih->fi, ih->di);
		DEBUGPCRF("computed Fi(%u) Di(%u) ratio: %d", ih->fi, ih->di, rc);
		usart->US_CR |= AT91C_US_RXDIS | AT91C_US_RSTRX;
		usart->US_FIDI = rc & 0x3ff;
		usart->US_CR |= AT91C_US_RXEN;
		set_atr_state(ih, ATR_S_WAIT_TS);
	} else if (new_state == ISO7816_S_WAIT_READER) {
		/* CLA INS P1 P2 LEN */
		ih->apdu_len = 5;
		ih->apdu_idx = 0;
	} else if (new_state == ISO7816_S_WAIT_CARD) {
		/* 8.2.2 procedure bytes sent by the card */
		/* FIXME: NULL byte and similar oddities */
		ih->apdu_len += 2;
	} 

	if (ih->state == new_state)
		return;

	DEBUGPCR("7816 state %u -> %u", ih->state, new_state);
	ih->state = new_state;
}

/* determine the next ATR state based on received interface byte */
static enum atr_state next_intb_state(struct iso7816_3_handle *ih, u_int8_t ch)
{
	switch (ih->atr_state) {
	case ATR_S_WAIT_TD:
	case ATR_S_WAIT_T0:
		ih->atr_last_td = ch;
		goto from_td;
	case ATR_S_WAIT_TC:
		goto from_tc;
	case ATR_S_WAIT_TB:
		goto from_tb;
	case ATR_S_WAIT_TA:
		if ((ih->atr_last_td & 0x0f) == 0) {
			/* This must be TA1 */
			ih->fi = ch >> 4;
			ih->di = ch & 0xf;
			DEBUGPCR("found Fi=%u Di=%u", ih->fi, ih->di);
		}
		goto from_ta;
	default:
		DEBUGPCR("something wrong, old_state != TA");
		return ATR_S_WAIT_TCK;
	}

from_td:
	if (ih->atr_last_td & 0x10)
		return ATR_S_WAIT_TA;
from_ta:
	if (ih->atr_last_td & 0x20)
		return ATR_S_WAIT_TB;
from_tb:
	if (ih->atr_last_td & 0x40)
		return ATR_S_WAIT_TC;
from_tc:
	if (ih->atr_last_td & 0x80)
		return ATR_S_WAIT_TD;

	return ATR_S_WAIT_HIST;
}

/* process an incomng ATR byte */
static enum iso7816_3_state
process_byte_atr(struct iso7816_3_handle *ih, u_int8_t byte)
{
	int rc;

	/* add byte to ATR buffer */
	ih->atr[ih->atr_idx] = byte;
	ih->atr_idx++;

	switch (ih->atr_state) {
	case ATR_S_WAIT_TS:
		/* FIXME: if we don't have the RST line we might get this */
		if (byte == 0) {
			ih->atr_idx--;
			break;
		}
		/* FIXME: check inverted logic */
		set_atr_state(ih, ATR_S_WAIT_T0);
		break;
	case ATR_S_WAIT_T0:
		ih->atr_hist_len = byte & 0xf;
		set_atr_state(ih, next_intb_state(ih, byte & 0xf0));
		break;
	case ATR_S_WAIT_TA:
	case ATR_S_WAIT_TB:
	case ATR_S_WAIT_TC:
	case ATR_S_WAIT_TD:
		set_atr_state(ih, next_intb_state(ih, byte));
		break;
	case ATR_S_WAIT_HIST:
		ih->atr_hist_len--;
		if (ih->atr_hist_len == 0)
			set_atr_state(ih, ATR_S_WAIT_TCK);
		break;
	case ATR_S_WAIT_TCK:
		/* FIXME: process TCK */
		set_atr_state(ih, ATR_S_DONE);
		/* FIXME: update Fi/Di */
		rc = compute_fidi_ratio(ih->fi, ih->di);
		if (rc > 0 && rc < 0x400) {
			DEBUGPCR("computed FiDi ratio %d", rc);
			/* update baud rate generator in UART */
			usart->US_CR |= AT91C_US_RXDIS| AT91C_US_RSTRX;
			usart->US_FIDI = rc & 0x3ff;
			usart->US_CR |= AT91C_US_RXEN;
		} else
			DEBUGPCRF("computed FiDi ratio %d unsupported", rc);
		return ISO7816_S_WAIT_READER;
	}

	return ISO7816_S_IN_ATR;
}

/* process an incomng byte from the reader */
static enum iso7816_3_state
process_byte_reader(struct iso7816_3_handle *ih, u_int8_t byte)
{
	/* add response length to total number of expected bytes */
	if (ih->apdu_idx == 4)
		ih->apdu_len += byte;

	ih->apdu_idx++;
	
	/* once we have received all bytes, transition to card response */
	if (ih->apdu_idx == ih->apdu_len)
		return ISO7816_S_WAIT_CARD;

	return ISO7816_S_WAIT_READER;
}

/* process an incomng byte from the card */
static enum iso7816_3_state
process_byte_card(struct iso7816_3_handle *ih, u_int8_t byte)
{
	ih->apdu_idx++;
	
	/* once we have received all bytes, apdu is finished */
	if (ih->apdu_idx == ih->apdu_len)
		return ISO7816_S_WAIT_READER;

	return ISO7816_S_WAIT_CARD;
}


void process_byte(struct iso7816_3_handle *ih, u_int8_t byte)
{
	int new_state = -1;

	switch (ih->state) {
	case ISO7816_S_RESET:
		break;
	case ISO7816_S_WAIT_ATR:
	case ISO7816_S_IN_ATR:
		new_state = process_byte_atr(ih, byte);
		break;
	case ISO7816_S_WAIT_READER:
		new_state = process_byte_reader(ih, byte);
		break;
	case ISO7816_S_WAIT_CARD:
		new_state = process_byte_card(ih, byte);
		break;
	}

	if (new_state != -1)
		set_state(ih, new_state);
}

static __ramfunc void usart_irq(void)
{
	u_int32_t csr = usart->US_CSR;
	u_int8_t octet;

	//DEBUGP("USART IRQ, CSR=0x%08x\n", csr);

	if (csr & AT91C_US_RXRDY) {
		/* at least one character received */
		octet = usart->US_RHR & 0xff;
		DEBUGP("%02x ", octet);
		process_byte(&isoh, octet);
	}

	if (csr & AT91C_US_TXRDY) {
		/* nothing to transmit anymore */
	}

	if (csr & (AT91C_US_PARE|AT91C_US_FRAME|AT91C_US_OVRE)) {
		/* some error has occurrerd */
	}
}

/* handler for the RST input pin state change */
static void reset_pin_irq(u_int32_t pio)
{
	if (!AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, pio)) {
		DEBUGPCR("nRST");
		set_state(&isoh, ISO7816_S_RESET);
	} else {
		DEBUGPCR("RST");
		set_state(&isoh, ISO7816_S_WAIT_ATR);
	}
}

void iso_uart_dump(void)
{
	u_int32_t csr = usart->US_CSR;

	DEBUGPCR("USART CSR=0x%08x", csr);
}

void iso_uart_rst(unsigned int state)
{
	DEBUGPCR("USART set nRST set state=%u", state);
	switch (state) {
	case 0:
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_nRST);
		AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_nRST);
		break;
	case 1:
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_nRST);
		AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, SIMTRACE_PIO_nRST);
		break;
	default:
		AT91F_PIO_CfgInput(AT91C_BASE_PIOA, SIMTRACE_PIO_nRST);
		break;
	}
}

void iso_uart_rx_mode(void)
{
	DEBUGPCR("USART Entering Rx Mode");
	/* Enable receive interrupts */
	usart->US_IER = AT91C_US_RXRDY | AT91C_US_OVRE | AT91C_US_FRAME |
			AT91C_US_PARE | AT91C_US_NACK | AT91C_US_ITERATION;


	/* call interrupt handler once to set initial state RESET / ATR */
	reset_pin_irq(SIMTRACE_PIO_nRST);
}

void iso_uart_clk_master(unsigned int master)
{
	DEBUGPCR("USART Clock Master %u", master);
	if (master) {
		usart->US_MR = AT91C_US_USMODE_ISO7816_0 | AT91C_US_CLKS_CLOCK |
				AT91C_US_CHRL_8_BITS | AT91C_US_NBSTOP_1_BIT |
				AT91C_US_CKLO;
		usart->US_BRGR = (0x0000 << 16) | 16;
	} else {
		usart->US_MR = AT91C_US_USMODE_ISO7816_0 | AT91C_US_CLKS_EXT |
				AT91C_US_CHRL_8_BITS | AT91C_US_NBSTOP_1_BIT |
				AT91C_US_CKLO;
		usart->US_BRGR = (0x0000 << 16) | 0x0001;
	}
}

void iso_uart_init(void)
{
	DEBUGPCR("USART Initializing");

	/* make sure we get clock from the power management controller */
	AT91F_US0_CfgPMC();

	/* configure all 3 signals as input */
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, SIMTRACE_PIO_IO, SIMTRACE_PIO_CLK);
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, SIMTRACE_PIO_nRST);

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_US0,
			      OPENPCD_IRQ_PRIO_USART,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &usart_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_US0);

	usart->US_CR = AT91C_US_RXDIS | AT91C_US_TXDIS | (AT91C_US_RSTRX | AT91C_US_RSTTX);
	/* FIXME: wait for some time */
	usart->US_CR = AT91C_US_RXDIS | AT91C_US_TXDIS;

	/* ISO7816 T=0 mode with external clock input */
	usart->US_MR = AT91C_US_USMODE_ISO7816_0 | AT91C_US_CLKS_EXT | 
			AT91C_US_CHRL_8_BITS | AT91C_US_NBSTOP_1_BIT |
			AT91C_US_CKLO;

	/* Disable all interrupts */
	usart->US_IDR = 0xff;
	/* Clock Divider = 1, i.e. no division of SCLK */
	usart->US_BRGR = (0x0000 << 16) | 0x0001;
	/* Disable Receiver Time-out */
	usart->US_RTOR = 0;
	/* Disable Transmitter Timeguard */
	usart->US_TTGR = 0;

	pio_irq_register(SIMTRACE_PIO_nRST, &reset_pin_irq);
	AT91F_PIO_CfgInputFilter(AT91C_BASE_PIOA, SIMTRACE_PIO_nRST);
	pio_irq_enable(SIMTRACE_PIO_nRST);
}
