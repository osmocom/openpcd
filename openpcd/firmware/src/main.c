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

//#include "board.h"
#include "dbgu.h"
#include "pcd_enumerate.h"

#define MSG_SIZE 				1000
#if 0
#define USART_COM

#if defined(__WinARM__) && !defined(UART_COM)
#warning "make sure syscalls.c is added to the source-file list (see makefile)"
#endif
#endif

//* external function

extern void Usart_init(void);
extern void AT91F_US_Put(char *buffer);	// \arg pointer to a string ending by \0
extern void Trace_Toggel_LED(unsigned int led);

struct _AT91S_CDC pCDC;

//*----------------------------------------------------------------------------
//* \fn    AT91F_USB_Open
//* \brief This function Open the USB device
//*----------------------------------------------------------------------------
void AT91F_USB_Open(void)
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
	AT91F_CDC_Open(&pCDC, AT91C_BASE_UDP);
}

//*--------------------------------------------------------------------------------------
//* Function Name       : main
//* Object              :
//*--------------------------------------------------------------------------------------
int main(void)
{
	char data[MSG_SIZE];
	unsigned int length;

#ifndef USART_COM
	char message[30];
	// Init trace DBGU
	AT91F_DBGU_Init();
	AT91F_DBGU_Printk
	    ("\n\r-I- Basic USB loop back\n\r 0) Set Pull-UP 1) Clear Pull UP\n\r");
#else
	// Set Usart in interrupt
	AT91F_DBGU_Init();
	Usart_init();
	AT91F_DBGU_Printk("\n\r-I- Basic USART USB\n\r");

#endif

	// Enable User Reset and set its minimal assertion to 960 us
	AT91C_BASE_RSTC->RSTC_RMR =
	    AT91C_RSTC_URSTEN | (0x4 << 8) | (unsigned int)(0xA5 << 24);

	// Init USB device
	AT91F_USB_Open();
	// Init USB device
	while (1) {
		// Check enumeration
		if (pCDC.IsConfigured(&pCDC)) {
#ifndef USART_COM
			// Loop
			length = pCDC.Read(&pCDC, data, MSG_SIZE);
			pCDC.Write(&pCDC, data, length);
			/// mt sprintf(message,"-I- Len %d:\n\r",length);
			siprintf(message, "-I- Len %d:\n\r", length);
			// send char
			AT91F_DBGU_Frame(message);
#else
			// Loop
			length = pCDC.Read(&pCDC, data, MSG_SIZE);
			data[length] = 0;
			Trace_Toggel_LED(LED1);
			AT91F_US_Put(data);
			/// AT91F_DBGU_Frame(data);
#endif
		}
	}
}
