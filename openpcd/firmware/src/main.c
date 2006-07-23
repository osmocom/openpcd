//*--------------------------------------------------------------------------------------
//*      ATMEL Microcontroller Software Support  -  ROUSSET  -
//*--------------------------------------------------------------------------------------
//* The software is delivered "AS IS" without warranty or condition of any
//* kind, either express, implied or statutory. This includes without
//* limitation any warranty or condition with respect to merchantability or
//* fitness for any particular purpose, or against the infringements of
//* intellectual property rights of others.
//*--------------------------------------------------------------------------------------
//* File Name           : main.c
//* Object              : Testr USB device
//* Translator          :
//* 1.0 19/03/01 HI     : Creation
//* 1.1 02/10/02 FB     : Add on Svc DataFlash
//* 1.2 13/Sep/04 JPP   : Add DBGU
//* 1.3 16/Dec/04 JPP   : Add USART and enable reset
//* 1.4 27/Apr/05 JPP   : Unset the USART_COM and suppress displaying data
//*--------------------------------------------------------------------------------------

#include <errno.h>
#include <string.h>
#include <include/lib_AT91SAM7.h>
#include <include/openpcd.h>
#include "dbgu.h"
#include "rc632.h"
#include "led.h"
#include "pcd_enumerate.h"
#include "openpcd.h"

#define MSG_SIZE 				1000
#if 0
#define USART_COM

#if defined(__WinARM__) && !defined(UART_COM)
#warning "make sure syscalls.c is added to the source-file list (see makefile)"
#endif
#endif

//*----------------------------------------------------------------------------
static void AT91F_USB_Open(void)
{
	// Set the PLL USB Divider
	AT91C_BASE_CKGR->CKGR_PLLR |= AT91C_CKGR_USBDIV_1;

	// Specific Chip USB Initialisation
	// Enables the 48MHz USB clock UDPCK and System Peripheral USB Clock
	AT91C_BASE_PMC->PMC_SCER = AT91C_PMC_UDP;
	AT91C_BASE_PMC->PMC_PCER = (1 << AT91C_ID_UDP);

	// Enable UDP PullUp (USB_DP_PUP) : enable & Clear of the corresponding PIO
	// Set in PIO mode and Configure in Output
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, AT91C_PIO_UDP_PUP);
	// Clear for set the Pul up resistor
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, AT91C_PIO_UDP_PUP);

	// CDC Open by structure initialization
	AT91F_CDC_Open(AT91C_BASE_UDP);
}

static int usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];
	u_int16_t len = rctx->rx.tot_len;

	DEBUGP("usb_in ");

	if (len < sizeof(*poh))
		return -EINVAL;

	//data_len = ntohs(poh->len);

	memcpy(pih, poh, sizeof(*poh));
	rctx->tx.tot_len = sizeof(*poh);

	switch (poh->cmd) {
	case OPENPCD_CMD_READ_REG:
		DEBUGP("READ REG(0x%02x) ", poh->reg);
		pih->val = rc632_reg_read(poh->reg);
		break;
	case OPENPCD_CMD_READ_FIFO:
		DEBUGP("READ FIFO(len=%u) ", poh->len);
		pih->len = rc632_fifo_read(poh->len, pih->data);
		rctx->tx.tot_len += pih->len;
		break;
	case OPENPCD_CMD_WRITE_REG:
		DEBUGP("WRITE_REG(0x%02x, 0x%02x) ", poh->reg, poh->val);
		rc632_reg_write(poh->reg, poh->val);
		break;
	case OPENPCD_CMD_WRITE_FIFO:
		DEBUGP("WRITE FIFO(len=%u) ", poh->len);
		if (len - sizeof(*poh) < poh->len)
			return -EINVAL;
		rc632_fifo_write(poh->len, poh->data);
		break;
	case OPENPCD_CMD_READ_VFIFO:
		DEBUGP("READ VFIFO ");
		DEBUGP("NOT IMPLEMENTED YET ");
		break;
	case OPENPCD_CMD_WRITE_VFIFO:
		DEBUGP("WRITE VFIFO ");
		DEBUGP("NOT IMPLEMENTED YET ");
		break;
	case OPENPCD_CMD_REG_BITS_CLEAR:
		DEBUGP("CLEAR BITS ");
		pih->val = rc632_clear_bits(poh->reg, poh->val);
		break;
	case OPENPCD_CMD_REG_BITS_SET:
		DEBUGP("SET BITS ");
		pih->val = rc632_set_bits(poh->reg, poh->val);
		break;
	case OPENPCD_CMD_SET_LED:
		DEBUGP("SET LED(%u,%u) ", poh->reg, poh->val);
		led_switch(poh->reg, poh->val);
		break;
	default:
		return -EINVAL;
	}
	DEBUGPCRF("calling UDP_Write");
	AT91F_UDP_Write(0, &rctx->tx.data[0], rctx->tx.tot_len);
	DEBUGPCRF("usb_in: returning to main");
}

#define DEBUG_TOGGLE_LED

int main(void)
{
	int i;

	led_init();
	// Init trace DBGU
	AT91F_DBGU_Init();
	rc632_init();
	
	// Enable User Reset and set its minimal assertion to 960 us
	AT91C_BASE_RSTC->RSTC_RMR =
	    AT91C_RSTC_URSTEN | (0x4 << 8) | (unsigned int)(0xA5 << 24);

	// Init USB device
	AT91F_USB_Open();

#ifdef DEBUG_CLOCK_PA6
	AT91F_PMC_EnablePCK(AT91C_BASE_PMC, 0, AT91C_PMC_CSS_PLL_CLK);
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, AT91C_PA6_PCK0);
#endif

	led_switch(1, 1);

	// Init USB device
	while (1) {
		struct req_ctx *rctx;

#ifdef DEBUG_TOGGLE_LED
		/* toggle LEDs */
		led_toggle(1);
		led_toggle(2);
#endif

		for (rctx = req_ctx_find_busy(); rctx; 
		     rctx = req_ctx_find_busy()) {
		     	DEBUGPCRF("found used ctx %u: len=%u", 
				req_ctx_num(rctx), rctx->rx.tot_len);
			usb_in(rctx);
			req_ctx_put(rctx);
		}

		/* busy-wait for led toggle */
		for (i = 0; i < 0x2fffff; i++) {}
	}
}
