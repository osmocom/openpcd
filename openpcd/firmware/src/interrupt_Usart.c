//*----------------------------------------------------------------------------
//*      ATMEL Microcontroller Software Support  -  ROUSSET  -
//*----------------------------------------------------------------------------
//* The software is delivered "AS IS" without warranty or condition of any
//* kind, either express, implied or statutory. This includes without
//* limitation any warranty or condition with respect to merchantability or
//* fitness for any particular purpose, or against the infringements of
//* intellectual property rights of others.
//*----------------------------------------------------------------------------
//* File Name           : interrupt_Usart.c
//* Object              : USART Interrupt Management
//*
//* 1.0 14/Dec/04 JPP   : Creation
//* 1.1 29/Aug/05 JPP   : Update AIC definion
//*----------------------------------------------------------------------------


// Include Standard LIB  files
#include "Board.h"

#include "cdc_enumerate.h"

#define USART_INTERRUPT_LEVEL		1

AT91PS_USART COM0;
#define USART_BAUD_RATE 		115200

extern struct _AT91S_CDC 	pCDC;
static char buff_rx[100];
static char buff_rx1[100];
unsigned int first =0;
//*------------------------- Internal Function --------------------------------

//*----------------------------------------------------------------------------
//* Function Name       : Trace_Toggel_LED
//* Object              : Toggel a LED
//*----------------------------------------------------------------------------
void Trace_Toggel_LED (unsigned int Led)
{
    if ( (AT91F_PIO_GetInput(AT91C_BASE_PIOA) & Led ) == Led )
    {
        AT91F_PIO_ClearOutput( AT91C_BASE_PIOA, Led );
    }
    else
    {
        AT91F_PIO_SetOutput( AT91C_BASE_PIOA, Led );
    }
}
//*------------------------- Interrupt Function -------------------------------

//*----------------------------------------------------------------------------
//* Function Name       : Usart_c_irq_handler
//* Object              : C handler interrupt function called by the interrupts
//*                       assembling routine
//*----------------------------------------------------------------------------
void Usart_c_irq_handler(void)
{
	AT91PS_USART USART_pt = COM0;
	unsigned int status;

	//* get Usart status register and active interrupt
	status = USART_pt->US_CSR ;
        status &= USART_pt->US_IMR;

	if ( status & AT91C_US_RXBUFF){
	//* Toggel LED
 	Trace_Toggel_LED( LED3) ;
	//* transfert the char to DBGU
	 if ( first == 0){
 	     COM0->US_RPR = (unsigned int) buff_rx1;
	     COM0->US_RCR = 100;
 	     pCDC.Write(&pCDC, buff_rx,100);
 	     first =1;
	   }else{
	     COM0->US_RPR = (unsigned int) buff_rx;
	     COM0->US_RCR = 100;
	     pCDC.Write(&pCDC, buff_rx1,100);
	     first=0;
	   }
	}
//* Check error
	
	if ( status & AT91C_US_TIMEOUT){
	 Trace_Toggel_LED( LED4) ;
	 status = 100 - COM0->US_RCR;
	 if  (status !=0){
 	   if ( first == 0){
		COM0->US_RPR = (unsigned int) buff_rx1;
		COM0->US_RCR = 100;
 	        pCDC.Write(&pCDC, buff_rx,status);
 	        first =1;
	   }else{
	        COM0->US_RPR = (unsigned int) buff_rx;
	        COM0->US_RCR = 100;
	        pCDC.Write(&pCDC, buff_rx1,status);
	        first=0;
	    }
            COM0->US_CR = AT91C_US_STTTO;
          }
	}
	//* Reset the satus bit for error
	 USART_pt->US_CR = AT91C_US_RSTSTA;
}
//*-------------------------- External Function -------------------------------

//*----------------------------------------------------------------------------
//* \fn    AT91F_US_Printk
//* \brief This function is used to send a string through the US channel
//*----------------------------------------------------------------------------
void AT91F_US_Put( char *buffer) // \arg pointer to a string ending by \0
{
	while(*buffer != '\0') {
		while (!AT91F_US_TxReady(COM0));
		AT91F_US_PutChar(COM0, *buffer++);
	}
}

//*----------------------------------------------------------------------------
//* Function Name       : Usart_init
//* Object              : USART initialization
//* Input Parameters    : none
//* Output Parameters   : TRUE
//*----------------------------------------------------------------------------
void Usart_init ( void )
//* Begin
{
   // Led init
   // First, enable the clock of the PIOB
     AT91F_PMC_EnablePeriphClock ( AT91C_BASE_PMC, 1 << AT91C_ID_PIOA ) ;
   //* to be outputs. No need to set these pins to be driven by the PIO because it is GPIO pins only.
     AT91F_PIO_CfgOutput( AT91C_BASE_PIOA, LED_MASK ) ;
   //* Clear the LED's.
    AT91F_PIO_SetOutput( AT91C_BASE_PIOA, LED_MASK ) ;
   //* Set led 1e LED's.
    AT91F_PIO_ClearOutput( AT91C_BASE_PIOA, LED1 ) ;


    COM0= AT91C_BASE_US0;
    //* Define RXD and TXD as peripheral
    // Configure PIO controllers to periph mode
    AT91F_PIO_CfgPeriph(
	 AT91C_BASE_PIOA, // PIO controller base address
	 ((unsigned int) AT91C_PA5_RXD0    ) |
	 ((unsigned int) AT91C_PA6_TXD0    ) , // Peripheral A
	 0 ); // Peripheral B

    //* First, enable the clock of the PIOB
    AT91F_PMC_EnablePeriphClock ( AT91C_BASE_PMC, 1<<AT91C_ID_US0 ) ;

    //* Usart Configure
    AT91F_US_Configure (COM0, MCK,AT91C_US_ASYNC_MODE,USART_BAUD_RATE , 0);

    //* Enable usart
    COM0->US_CR = AT91C_US_RXEN | AT91C_US_TXEN;

    //* open Usart interrupt
    AT91F_AIC_ConfigureIt (AT91C_BASE_AIC, AT91C_ID_US0, USART_INTERRUPT_LEVEL,
                           AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, Usart_c_irq_handler);
    AT91F_AIC_EnableIt (AT91C_BASE_AIC, AT91C_ID_US0);
    // Set the PDC
    AT91F_PDC_Open (AT91C_BASE_PDC_US0);
    COM0->US_RPR = (unsigned int) buff_rx;
    COM0->US_RCR = 100;
    first = 0;
    COM0->US_RTOR = 10;
    //* Enable USART IT error and AT91C_US_ENDRX
     AT91F_US_EnableIt(COM0,AT91C_US_RXBUFF | AT91C_US_TIMEOUT );
//* End
}
