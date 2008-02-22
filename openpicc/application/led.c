/***************************************************************
 *
 * OpenBeacon.org - LED support
 *
 * Copyright 2007 Milosch Meriac <meriac@openbeacon.de>
 *
 ***************************************************************

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/
#include <FreeRTOS.h>
#include <AT91SAM7.h>
#include <lib_AT91SAM7.h>
#include <string.h>
#include <board.h>
#include <task.h>
#include "led.h"
/**********************************************************************/

#define BLINK_TIME 5
#define PWM_PERIOD 1000
struct {int channel; AT91PS_PWMC_CH periph_a, periph_b;} pwm_channels[] = {
		[0] = {0, AT91C_BASE_PWMC_CH0, 0},
		[1] = {1, AT91C_BASE_PWMC_CH1, 0},
		[2] = {2, AT91C_BASE_PWMC_CH2, 0},
		[7] = {3, 0, AT91C_BASE_PWMC_CH3},
		[11] = {0, 0, AT91C_BASE_PWMC_CH0},
		[12] = {1, 0, AT91C_BASE_PWMC_CH1},
		[13] = {2, 0, AT91C_BASE_PWMC_CH2},
		[14] = {3, 0, AT91C_BASE_PWMC_CH3},
		[23] = {0, 0, AT91C_BASE_PWMC_CH0},
		[24] = {1, 0, AT91C_BASE_PWMC_CH1},
		[25] = {2, 0, AT91C_BASE_PWMC_CH2},
};

static AT91PS_PWMC_CH find_channel(unsigned int led)
{
	int pos = ffs(led);
	if(pos==0) return 0 ;
	
	if(pwm_channels[pos-1].periph_a != 0) {
		return pwm_channels[pos-1].periph_a;
	} else if(pwm_channels[pos-1].periph_b != 0) {
		return pwm_channels[pos-1].periph_b;
	} else
		return 0;
}

// Brightness is in per thousand
void vLedSetBrightness(unsigned int led, int brightness)
{
	AT91PS_PWMC_CH channel = find_channel(led);
	if(channel==0) return;
	
	if(brightness < 1) brightness = 1;
	if(brightness > 999) brightness = 999;
	channel->PWMC_CUPDR = (brightness * PWM_PERIOD) / 1000; 
}

void vLedSetRed(bool_t on)
{
    if(on)
    	AT91F_PIO_ClearOutput( AT91C_BASE_PIOA, LED_RED );
    else
        AT91F_PIO_SetOutput( AT91C_BASE_PIOA, LED_RED );
}
/**********************************************************************/

void vLedBlinkRed(void)
{
	volatile int i=0;
	vLedSetRed(1);
	for(i=0; i<BLINK_TIME; i++) {vLedSetRed(0);vLedSetRed(1);}
	vLedSetRed(0);
}
/**********************************************************************/

void vLedSetGreen(bool_t on)
{
    if(on)
    	AT91F_PIO_ClearOutput( AT91C_BASE_PIOA, LED_GREEN );
    else
        AT91F_PIO_SetOutput( AT91C_BASE_PIOA, LED_GREEN );
}
/**********************************************************************/

void vLedBlinkGreen(void)
{
	volatile int i=0;
	vLedSetGreen(1);
	for(i=0; i<BLINK_TIME; i++) {vLedSetGreen(0);vLedSetGreen(1);}
	vLedSetGreen(0);
}
/**********************************************************************/

void vLedHaltBlinking(int reason)
{
    volatile u_int32_t i=0;
    while(1)
    {
        AT91F_PIO_ClearOutput( AT91C_BASE_PIOA, LED_RED | LED_GREEN );
        for(i=0; i<MCK/40; i++) ;
	
	switch(reason) {
		case 1:
			vLedSetGreen(1);
			vLedSetRed(0);
			break;
		case 2:
			vLedSetGreen(0);
			vLedSetRed(1);
			break;
		case 3:
			vLedSetGreen(1);
			vLedSetRed(1);
			break;
		case 0:
		default:
			vLedSetGreen(0);
			vLedSetRed(0);
			break;
	}
        for(i=0; i<MCK/20; i++) ;
	
        AT91F_PIO_SetOutput( AT91C_BASE_PIOA, LED_RED | LED_GREEN );
        for(i=0; i<MCK/40; i++) ;
    }
}
/**********************************************************************/

static void init_led_pwm(unsigned int led)
{
	int pos = ffs(led);
	if(pos==0) return;
	AT91PS_PWMC_CH channel;
	
	// Set PIO mapping
	if(pwm_channels[pos-1].periph_a != 0) {
		AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, led, 0);
		channel = pwm_channels[pos-1].periph_a;
	} else if(pwm_channels[pos-1].periph_b != 0) {
		AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, led);
		channel = pwm_channels[pos-1].periph_b;
	} else
		return;
	
	// Configure channel: clock = MCK/128, period = PWM_PERIOD, CPOL=1, duty cycle variable
	channel->PWMC_CMR = 0x7 | AT91C_PWMC_CPOL;
	channel->PWMC_CDTYR = 0;
	channel->PWMC_CPRDR = PWM_PERIOD;
	
	AT91C_BASE_PWMC->PWMC_ENA = (1<<pwm_channels[pos-1].channel);
	vLedSetBrightness(led, 0);
}

void vLedInit(void)
{
    // turn off LED's 
    AT91F_PIO_CfgOutput( AT91C_BASE_PIOA, LED_RED | LED_GREEN );
    AT91F_PIO_SetOutput( AT91C_BASE_PIOA, LED_RED | LED_GREEN );
	
    AT91F_PMC_EnablePeriphClock(AT91C_BASE_PMC,
				    ((unsigned int) 1 << AT91C_ID_PWMC));
    /* Do not configure green for PWM since it's being used from FIQ */
    //init_led_pwm(LED_GREEN);
    init_led_pwm(LED_RED);
}
