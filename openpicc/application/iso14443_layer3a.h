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

/******************** RX ************************************/
/* measured TF->FIQ->SSC TX start delay (~3.480us) in carrier cycles */
#define TF_FIQ_SSC_DELAY 47
//#define TF_FIQ_SSC_DELAY 40
#define FALLING_EDGE_DETECTION_DELAY 12
extern volatile int fdt_offset;
/* standard derived magic values */
#define ISO14443A_FDT_SLOTLEN 128
#define ISO14443A_FDT_OFFSET_1 84
#define ISO14443A_FDT_OFFSET_0 20
#define ISO14443A_FDT_SHORT_1	(ISO14443A_FDT_SLOTLEN*9 + ISO14443A_FDT_OFFSET_1 -FALLING_EDGE_DETECTION_DELAY +fdt_offset -TF_FIQ_SSC_DELAY)
#define ISO14443A_FDT_SHORT_0	(ISO14443A_FDT_SLOTLEN*9 + ISO14443A_FDT_OFFSET_0 -FALLING_EDGE_DETECTION_DELAY +fdt_offset -TF_FIQ_SSC_DELAY)

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
/* This is wrong: a short frame is 1 start bit, 7 data bits, 1 stop bit 
 * followed by no modulation for one bit period. The start bit is 'eaten' in 
 * SSC Compare 0, but the remaining 7+1+1 bit durations must be sampled and
 * compared. At four times oversampling this would be 9*4=36 samples, which is 
 * more than one SSC transfer. You'd have to use two transfers of 18 samples
 * each and modify the comparison code accordingly. 
 * Since four times oversampling doesn't work reliably anyway (every second
 * sample is near an edge and might sample 0 or 1) this doesn't matter for now.*/
#error Four times oversampling is broken, see comments in code 

#else
/* definitions for two-times oversampling */
#define REQA	0x4929
#define WUPA	0x2249

#define ISO14443A_SOF_SAMPLE	0x01
#define ISO14443A_SOF_LEN	2
#define ISO14443A_SHORT_LEN     18

#endif

/* A short frame should be received in one single SSC transfer */
#if (ISO14443A_SHORT_LEN <= 8)
/* SSC transfer size in bits */
#define ISO14443A_SHORT_TRANSFER_SIZE 8
#define ISO14443A_SHORT_TYPE u_int8_t

#elif (ISO14443A_SHORT_LEN <= 16)
#define ISO14443A_SHORT_TRANSFER_SIZE 16
#define ISO14443A_SHORT_TYPE u_int16_t

#elif (ISO14443A_SHORT_LEN <= 32)
#define ISO14443A_SHORT_TRANSFER_SIZE 32
#define ISO14443A_SHORT_TYPE u_int32_t

#else
#error ISO14443A_SHORT_LEN defined too big
#endif

/******************** TX ************************************/
/* in bytes, not counting parity */
#define MAXIMUM_FRAME_SIZE 256

typedef struct {
  enum { TYPE_A, TYPE_B } type;
  union {
  	struct {
  		enum { SHORT_FRAME, STANDARD_FRAME, AC_FRAME } format;
  		enum { PARITY, /* Calculate parity on the fly, ignore the parity field below */ 
  		       GIVEN_PARITY, /* Use the parity bits from the parity field below */  
  		       NO_PARITY, /* Don't send any parity */
  		} parity; 
  	} a;
  } parameters;
  u_int32_t numbytes;
  u_int8_t numbits, bit_offset;
  u_int8_t data[MAXIMUM_FRAME_SIZE];
  u_int8_t parity[MAXIMUM_FRAME_SIZE/8+1]; /* parity bit for data[x] is in parity[x/8] & (1<<(x%8)) */
} iso14443_frame;

extern const iso14443_frame ATQA_FRAME;

#endif /*ISO14443_LAYER3A_H_*/
