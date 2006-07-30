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

#include <sys/types.h>
#include <AT91SAM7.h>
#include <asm/atomic.h>
#include "src/openpcd.h"

#define AT91C_EP_OUT 1
#define AT91C_EP_OUT_SIZE 0x40
#define AT91C_EP_IN  2
#define AT91C_EP_IN_SIZE 0x40
#define AT91C_EP_INT  3

struct ep_ctx {
	atomic_t pkts_in_transit;
	void *ctx;
};


typedef struct _AT91S_CDC
{
	AT91PS_UDP pUdp;
	unsigned char currentConfiguration;
	unsigned char currentConnection;
	unsigned int  currentRcvBank;
	struct ep_ctx ep[4];
} AT91S_CDC, *AT91PS_CDC;

//* external function description

extern void udp_init(void);
u_int8_t AT91F_UDP_IsConfigured(void);

//u_int32_t AT91F_UDP_Write(u_int8_t irq, const unsigned char *pData, u_int32_t length);

extern int udp_refill_ep(int ep, struct req_ctx *rctx);
extern void udp_unthrottle(void);

#endif // CDC_ENUMERATE_H

