/* main_reqa - OpenPCD firmware for generating an endless loop of
 * ISO 14443-A REQA packets.
 *
 * If a response is received from the PICC, LED1 (Red) will be switched
 * on.  If no valid response has been received within the timeout of the
 * receiver, LED1 (Red) will be switched off.
 *
 */

#include "rc632.h"
#include "dbgu.h"
#include "led.h"
#include "trigger.h"
#include "pcd_enumerate.h"
#include <rfid_layer2_iso14443a.h>
#include <string.h>

void _init_func(void)
{
	//udp_init();
	trigger_init();
	rc632_init();
	DEBUGPCRF("turning on RF");
	rc632_turn_on_rf(RAH);
	DEBUGPCRF("initializing 14443A operation");
	rc632_iso14443a_init(RAH);
}


void _main_func(void)
{
	struct iso14443a_atqa atqa;

	memset(&atqa, 0, sizeof(atqa));

	trigger_pulse();

	if (rc632_iso14443a_transceive_sf(RAH, ISO14443A_SF_CMD_WUPA, &atqa) < 0) {
		DEBUGPCRF("error during transceive_sf");
		led_switch(1, 0);
	} else
		led_switch(1, 1);
	
	led_toggle(2);
}
