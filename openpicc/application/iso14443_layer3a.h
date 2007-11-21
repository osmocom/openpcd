#ifndef ISO14443_LAYER3A_H_
#define ISO14443_LAYER3A_H_

extern void iso14443_layer3a_state_machine (void *pvParameters);

enum ISO14443_STATES {
	STARTING_UP, /* Hardware has not been initialized, initialize hardware, go to power-off */
	POWERED_OFF, /* Card not in field, wait for PLL lock */
	IDLE,        /* Card in field and powered, wait for REQA or WUPA */
	READY,       /* Perform anticollision, wait for select */
	ACTIVE,      /* Selected */
	HALT,        /* Card halted and powered, wait for WUPA */
	READY_STAR,  /* Perform anticollision, wait for select */
	ACTIVE_STAR, /* Selected */
	ERROR,       /* Some unrecoverable error has occured */
};

#endif /*ISO14443_LAYER3A_H_*/
