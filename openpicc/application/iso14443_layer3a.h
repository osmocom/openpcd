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

#include "iso14443.h"

extern const u_int8_t ISO14443A_SHORT_FRAME_REQA[ISO14443A_SHORT_FRAME_COMPARE_LENGTH];
extern const u_int8_t ISO14443A_SHORT_FRAME_WUPA[ISO14443A_SHORT_FRAME_COMPARE_LENGTH];

/******************** TX ************************************/
/* Magic delay, don't know where it comes from */
//#define MAGIC_OFFSET -32
#define MAGIC_OFFSET -10
/* Delay from modulation till detection in SSC_DATA */
#define DETECTION_DELAY 11
/* See fdt_timinig.dia for these values */
#define MAX_TF_FIQ_ENTRY_DELAY 16
#define MAX_TF_FIQ_OVERHEAD 75 /* guesstimate */ 
extern volatile int fdt_offset;
/* standard derived magic values */
#define ISO14443A_FDT_SLOTLEN 128
#define ISO14443A_FDT_OFFSET_1 84
#define ISO14443A_FDT_OFFSET_0 20
#define ISO14443A_FDT_SHORT_1	(ISO14443A_FDT_SLOTLEN*9 + ISO14443A_FDT_OFFSET_1 +fdt_offset +MAGIC_OFFSET -DETECTION_DELAY)
#define ISO14443A_FDT_SHORT_0	(ISO14443A_FDT_SLOTLEN*9 + ISO14443A_FDT_OFFSET_0 +fdt_offset +MAGIC_OFFSET -DETECTION_DELAY)

extern const iso14443_frame ATQA_FRAME;

#endif /*ISO14443_LAYER3A_H_*/
