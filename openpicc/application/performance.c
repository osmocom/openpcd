/* T/C driver for performance measurements
 *  
 * Copyright 2008 Henryk Plötz <henryk@ploetzli.ch>
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

#include <FreeRTOS.h>
#include <AT91SAM7.h>
#include <openpicc.h>

#include <string.h>

#include "performance.h"
#include "cmd.h"

static AT91PS_TC tc_perf = AT91C_BASE_TC1;
static u_int32_t overruns = 0;

#define NUMBER_OF_CHECKPOINTS 20
static struct performance_checkpoint checkpoints[NUMBER_OF_CHECKPOINTS];
static int current_checkpoint;

static void __ramfunc tc_perf_irq(void) __attribute__ ((naked));
static void __ramfunc tc_perf_irq(void)
{
	portSAVE_CONTEXT();
	u_int32_t sr = tc_perf->TC_SR;
	if(sr & AT91C_TC_COVFS) overruns++;
	AT91F_AIC_AcknowledgeIt();
	portRESTORE_CONTEXT();
}

void performance_init(void)
{
	AT91F_PMC_EnablePeriphClock(AT91C_BASE_PMC, ((u_int32_t) 1 << AT91C_ID_TC1));
	/* clock is MCK/2, TIOA/B unconfigured, reset only on SWTRG, ignore Compare C */
	tc_perf->TC_CMR = AT91C_TC_CLKS_TIMER_DIV1_CLOCK;
	
	tc_perf->TC_CCR = AT91C_TC_CLKDIS;
	
	AT91F_AIC_ConfigureIt(AT91C_ID_TC1, AT91C_AIC_PRIOR_HIGHEST-1,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, (THandler)&tc_perf_irq);
	tc_perf->TC_IER = AT91C_TC_COVFS;
	AT91F_AIC_ClearIt(AT91C_ID_TC1);
	AT91F_AIC_EnableIt(AT91C_ID_TC1);
	
	memset(checkpoints, 0, sizeof(checkpoints));
}

inline void performance_start(void)
{
	memset(checkpoints, 0, sizeof(checkpoints));
	current_checkpoint = 0;
	overruns = 0;
	tc_perf->TC_CCR = AT91C_TC_SWTRG | AT91C_TC_CLKEN;
}

inline perf_time_t performance_get(void)
{
	u_int32_t cv = tc_perf->TC_CV;
	perf_time_t result = {
			.high = overruns,
			.low = cv,
	};
	return result;
}

inline perf_time_t performance_stop(void)
{
	perf_time_t result = performance_get();
	tc_perf->TC_CCR = AT91C_TC_CLKDIS;
	return result;
}

void performance_print(perf_time_t time)
{
	DumpUIntToUSB(time.high);
	DumpStringToUSB(":");
	DumpUIntToUSB(time.low);
}

perf_time_t performance_diff(perf_time_t a, perf_time_t b)
{
	// assert that a <= b
	if( (a.high > b.high) || (a.high == b.high && a.low > b.low) ) return performance_diff(b, a);
	perf_time_t result = { b.high - a.high, 0 };
	if(b.high == a.high) result.low = b.low - a.low;
	
	return result;
}

void performance_set_checkpoint(const char * const description)
{
	if(current_checkpoint < NUMBER_OF_CHECKPOINTS) {
		perf_time_t time = performance_get();
		checkpoints[current_checkpoint++] = (struct performance_checkpoint){
				.time = time,
				.description = description,
		};
	}
}

void performance_stop_report(void)
{
	perf_time_t _now = performance_stop();
	struct performance_checkpoint now = {
			.time = _now,
			.description = "end of data collection",
	};
	perf_time_t last = {0,0};
	int i;
	DumpStringToUSB("Performance report: \n\r");
	for(i = 0; i <= current_checkpoint; i++) {
		struct performance_checkpoint current = (i<current_checkpoint ? checkpoints[i] : now);
		
		DumpStringToUSB("\t");
		performance_print(current.time);
		DumpStringToUSB("   \t");
		
		if(last.high == 0 && last.low == 0) 
			DumpStringToUSB("       ");
		else
			performance_print(performance_diff(last, current.time));
		last = current.time;
		DumpStringToUSB("   \t");
		
		if(current.description)
			DumpStringToUSB(current.description);
		DumpStringToUSB("\n\r");
	}
}
