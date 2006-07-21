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
#include <include/lib_AT91SAM7S64.h>
#include <include/openpcd.h>
#include "dbgu.h"
#include "rc632.h"
#include "led.h"
#include "pcd_enumerate.h"

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
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, AT91C_PIO_PA16);
	// Clear for set the Pul up resistor
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, AT91C_PIO_PA16);

	// CDC Open by structure initialization
	AT91F_CDC_Open(AT91C_BASE_UDP);
}

static int usb_in(int len)
{
	struct openpcd_hdr *poh;
	struct openpcd_hdr *pih;
	u_int16_t data_len;

	if (len < sizeof(*poh))
		return -EINVAL;

	//data_len = ntohs(poh->len);

	memcpy(pih, poh, sizeof(*poh));

	switch (poh->cmd) {
	case OPENPCD_CMD_WRITE_REG:
		DEBUGP("WRITE_REG ");
		rc632_reg_write(poh->reg, poh->val);
		break;
	case OPENPCD_CMD_WRITE_FIFO:
		DEBUGP("WRITE FIFO ");
		if (len - sizeof(*poh) < data_len)
			return -EINVAL;
		rc632_fifo_write(data_len, poh->data);
		break;
	case OPENPCD_CMD_WRITE_VFIFO:
		DEBUGP("WRITE VFIFO ");
		break;
	case OPENPCD_CMD_READ_REG:
		DEBUGP("READ REG ");
		pih->val = rc632_reg_read(poh->reg);
		break;
	case OPENPCD_CMD_READ_FIFO:
		DEBUGP("READ FIFO ");
		pih->len = rc632_fifo_read(poh->len, pih->data);
		break;
	case OPENPCD_CMD_READ_VFIFO:
		DEBUGP("READ VFIFO ");
		break;
	case OPENPCD_CMD_SET_LED:
		DEBUGP("SET LED ");
		led_switch(poh->reg, poh->val);
		break;
	default:
		return -EINVAL;
	}
}

int main(void)
{
	int i, state = 0;

	// Init trace DBGU
	AT91F_DBGU_Init();
	AT91F_DBGU_Printk
	    ("\n\r-I- OpenPCD USB loop back\n\r 0) Set Pull-UP 1) Clear Pull UP\n\r");

	led_init();
	rc632_init();
	
	//printf("test 0x%02x\n\r", 123);

	// Enable User Reset and set its minimal assertion to 960 us
	AT91C_BASE_RSTC->RSTC_RMR =
	    AT91C_RSTC_URSTEN | (0x4 << 8) | (unsigned int)(0xA5 << 24);

	// Init USB device
	AT91F_USB_Open();

	// Init USB device
	while (1) {
		// Check enumeration
		if (AT91F_UDP_IsConfigured()) {

		}
		if (state == 0) {
			led_switch(1, 1);
			led_switch(2, 0);
			state = 1;
		} else {
			led_switch(1, 0);
			led_switch(2, 1);
			state = 0;
		}
		for (i = 0; i < 0x7fffff; i++) {}
	}
}
