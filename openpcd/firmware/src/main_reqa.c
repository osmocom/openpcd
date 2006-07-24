#include "rc632_highlevel.h"
#include "rc632.h"
#include "dbgu.h"
#include "trigger.h"

void _init_func(void)
{
	rc632_init();
	udp_init();
	trigger_init();
}


void _main_func(void)
{
	struct iso14443a_atqa atqa;

	memset(&atqa, 0, sizeof(atqua));

	trigger_pulse();

	if (rc632_iso14443a_transceive_sf(ISO14443A_SF_CMD_WUPA, &atqa)	< 0)
		DEBUGPCRF("error during transceive_sf");
	
}
