/***************************************************************
 *
 * OpenBeacon.org - LED support
 *
 * Copyright 2007 Milosch Meriac <meriac@openbeacon.de>
 * Copyright 2008 Henryk Plötz <henryk@ploetzli.ch>
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

#include "gammatable.inc"

/**********************************************************************/

#define BLINK_TIME 5
#define PWM_PERIOD GAMMA_MAX
const struct channel_definition {
	int channel_no;
	enum { PERIPH_NONE=0, PERIPH_A, PERIPH_B} periph;
	AT91PS_PWMC_CH channel;
} 
pwm_channels[] = {
		[0] =  {0, PERIPH_A,  AT91C_BASE_PWMC_CH0},
		[1] =  {1, PERIPH_A,  AT91C_BASE_PWMC_CH1},
		[2] =  {2, PERIPH_A,  AT91C_BASE_PWMC_CH2},
		[7] =  {3, PERIPH_B,  AT91C_BASE_PWMC_CH3},
		[11] = {0, PERIPH_B,  AT91C_BASE_PWMC_CH0},
		[12] = {1, PERIPH_B,  AT91C_BASE_PWMC_CH1},
		[13] = {2, PERIPH_B,  AT91C_BASE_PWMC_CH2},
		[14] = {3, PERIPH_B,  AT91C_BASE_PWMC_CH3},
		[23] = {0, PERIPH_B,  AT91C_BASE_PWMC_CH0},
		[24] = {1, PERIPH_B,  AT91C_BASE_PWMC_CH1},
		[25] = {2, PERIPH_B,  AT91C_BASE_PWMC_CH2},
};

static const struct channel_definition* find_channel(unsigned int led)
{
	unsigned int pos = ffs(led);
	if(pos==0) return 0;
	
	if(pos-1 >= sizeof(pwm_channels) / sizeof(pwm_channels[0]) ) return 0;
	if(pwm_channels[pos-1].periph == PERIPH_NONE) return 0;
	return &pwm_channels[pos-1];
}

// Brightness is in per thousand
void vLedSetBrightness(unsigned int led, int brightness)
{
	const struct channel_definition *c = find_channel(led);
	if(c==0) return;
	
	if(brightness < 0) brightness = 0;
	if(brightness > GAMMA_LEN-1) brightness = GAMMA_LEN-1;
	
	int duty = gammatable[brightness];
	if(duty > 1) {
		c->channel->PWMC_CUPDR = duty;
		if(c->periph == PERIPH_A) {
			AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, led, 0);
		} else if(c->periph == PERIPH_B) {
			AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, led);
		}
	} else {
	    vLedSet(led, 0);
	}
}

void vLedSet(int led, bool_t on)
{
    if(on)
    	AT91F_PIO_ClearOutput( AT91C_BASE_PIOA, led );
    else
        AT91F_PIO_SetOutput( AT91C_BASE_PIOA, led );
    AT91F_PIO_CfgOutput( AT91C_BASE_PIOA, led );
}

void vLedBlink(int led)
{
	volatile int i=0;
	vLedSet(led, 1);
	for(i=0; i<BLINK_TIME; i++) {vLedSet(led,0);vLedSet(led,1);}
	vLedSet(led, 0);	
}

/**********************************************************************/
void vLedSetRed(bool_t on)
{
	vLedSet(LED_RED, on);
}

void vLedBlinkRed(void)
{
	vLedBlink(LED_RED);
}
/**********************************************************************/

void vLedSetGreen(bool_t on)
{
	vLedSet(LED_GREEN, on);
}
/**********************************************************************/

void vLedBlinkGreen(void)
{
	vLedBlink(LED_GREEN);
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
	const struct channel_definition *c = find_channel(led);
	
	if(c==0) return;
	
	// Set PIO mapping
	if(c->periph == PERIPH_A) {
		AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, led, 0);
	} else if(c->periph == PERIPH_B) {
		AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, led);
	} else
		return;
	
	// Configure channel: clock = MCK, period = PWM_PERIOD, CPOL=1, duty cycle variable
	c->channel->PWMC_CMR = 0x0 | AT91C_PWMC_CPOL;
	c->channel->PWMC_CDTYR = 1;
	c->channel->PWMC_CPRDR = PWM_PERIOD;
	
	AT91C_BASE_PWMC->PWMC_ENA = (1<<(c->channel_no));
	vLedSetBrightness(led, 0);
}

void vLedInit(void)
{
    // turn off LED's 
    AT91F_PIO_CfgOutput( AT91C_BASE_PIOA, LED_RED | LED_GREEN );
    AT91F_PIO_SetOutput( AT91C_BASE_PIOA, LED_RED | LED_GREEN );
	
    AT91F_PMC_EnablePeriphClock(AT91C_BASE_PMC,
				    ((unsigned int) 1 << AT91C_ID_PWMC));
    
    init_led_pwm(LED_GREEN);
    init_led_pwm(LED_RED);
}
