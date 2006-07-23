//*----------------------------------------------------------------------------
//*      ATMEL Microcontroller Software Support  -  ROUSSET  -
//*----------------------------------------------------------------------------
//* The software is delivered "AS IS" without warranty or condition of any
//* kind, either express, implied or statutory. This includes without
//* limitation any warranty or condition with respect to merchantability or
//* fitness for any particular purpose, or against the infringements of
//* intellectual property rights of others.
//*----------------------------------------------------------------------------
//* File Name           : cdc_enumerate.h
//* Object              : Handle CDC enumeration
//*
//* 1.0 Apr 20 200 	: ODi Creation
//*----------------------------------------------------------------------------
#ifndef PCD_ENUMERATE_H
#define PCD_ENUMERATE_H

#include <include/AT91SAM7.h>
#include <include/types.h>

#define AT91C_EP_OUT 1
#define AT91C_EP_OUT_SIZE 0x40
#define AT91C_EP_IN  2
#define AT91C_EP_IN_SIZE 0x40
#define AT91C_EP_INT  3


typedef struct _AT91S_CDC
{
	AT91PS_UDP pUdp;
	unsigned char currentConfiguration;
	unsigned char currentConnection;
	unsigned int  currentRcvBank;
} AT91S_CDC, *AT91PS_CDC;

//* external function description

AT91PS_CDC AT91F_CDC_Open(AT91PS_UDP pUdp);
u_int8_t AT91F_UDP_IsConfigured(void);
u_int32_t AT91F_UDP_Write(u_int8_t irq, const char *pData, u_int32_t length);

#endif // CDC_ENUMERATE_H

