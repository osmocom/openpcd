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

/* standard derived magic values */
#define ISO14443A_FDT_SHORT_1	1236
#define ISO14443A_FDT_SHORT_0	1172

#ifdef FOUR_TIMES_OVERSAMPLING
/* definitions for four-times oversampling */
/* Sample values for the REQA and WUPA short frames */
#define REQA	0x10410441
#define WUPA	0x04041041

/* Start of frame sample for SSC compare 0 */
#define ISO14443A_SOF_SAMPLE	0x01
#define ISO14443A_SOF_LEN	4
/* Length in samples of a short frame */
#define ISO14443A_SHORT_LEN     32

#else
/* definitions for two-times oversampling */
#define REQA	0x18729
#define WUPA	0x2249

#define ISO14443A_SOF_SAMPLE	0x01
#define ISO14443A_SOF_LEN	2
#define ISO14443A_SHORT_LEN     16

#endif

#endif /*ISO14443_LAYER3A_H_*/
