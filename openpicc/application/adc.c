/* AT91 ADC for OpenPICC
 * (C) 2007 Henryk Plötz <henryk@ploetzli.ch> 
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


#include "openpicc.h"
#include "board.h"

#include <sys/types.h>
#include <lib_AT91SAM7.h>

static AT91S_ADC* adc = AT91C_BASE_ADC;

int adc_get_field_strength(void)
{
	adc->ADC_CR = AT91C_ADC_START;
	while( (adc->ADC_SR & AT91C_ADC_EOC4)==0) ;
	return adc->ADC_CDR4;
}

void adc_init(void)
{
	// PRESCAL = 4 -> ADC clock = MCLK / 10 = 4.8 MHz (max is 5 MHz at 10 bit)
	// Start time <20 us -> STARTUP=11
	// Track and hold time >600 ns -> SHTIM=2
	adc->ADC_MR = AT91C_ADC_TRGEN_DIS | AT91C_ADC_LOWRES_10_BIT | 
		AT91C_ADC_SLEEP_MODE | (4 << 8) | (11 << 16) | (2 << 24);
	adc->ADC_IDR = 0xfffff;
	adc->ADC_CHER = OPENPICC_ADC_FIELD_STRENGTH;
}
