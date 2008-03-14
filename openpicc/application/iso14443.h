#ifndef ISO14443_H_
#define ISO14443_H_

/******************** RX ************************************/
#define ISO14443_BITS_PER_SSC_TRANSFER 4

#ifdef FOUR_TIMES_OVERSAMPLING
/* definitions for four-times oversampling */
#define ISO14443A_SAMPLE_LEN    4
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
#define ISO14443A_SAMPLE_LEN    2

/* For SSC_MODE_ISO14443A_SHORT */
#define ISO14443A_SHORT_LEN     18
#define REQA	0x4929
#define WUPA	0x2249

#define ISO14443A_SOF_SAMPLE	0x01
#define ISO14443A_SOF_LEN	2

#define ISO14443A_EOF_SAMPLE    0x00
#define ISO14443A_EOF_LEN       7

/* For SSC_MODE_ISO14443A */
#define ISO14443A_SHORT_FRAME_COMPARE_LENGTH 2
#define _ISO14443A_SHORT_FRAME_REQA { 0x29, 0x49 }
#define _ISO14443A_SHORT_FRAME_WUPA { 0x49, 0x22 }
// FIXME not correct. This should be compare_length == 3 (which is 9 samples at 4 per compare), but this
// needs enhanced ssc irq code to transfer the last read (incomplete) data from the ssc holding 
// register to the buffer 

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


/******************** RX and TX *****************************/
/* in bytes, not counting parity */
#define MAXIMUM_FRAME_SIZE 256
#define ISO14443A_MAX_RX_FRAME_SIZE_IN_BITS (MAXIMUM_FRAME_SIZE*9 +2)

typedef enum {
	FRAME_FREE=0,    /* Frame is free */
	FRAME_PENDING,   /* Frame is currently filled by Rx */
	FRAME_FULL,      /* Frame has been filled by Rx */
	FRAME_PROCESSING,/* The frame is currently processed by the consumer */
} iso14443_frame_state_t;


typedef struct {
  enum { TYPE_A, TYPE_B } type;
  iso14443_frame_state_t state;
  union {
  	struct {
  		enum { SHORT_FRAME, STANDARD_FRAME, AC_FRAME } format;
  		enum { PARITY, /* Calculate parity on the fly, ignore the parity field below */ 
  		       GIVEN_PARITY, /* Use the parity bits from the parity field below */  
  		       NO_PARITY, /* Don't send any parity */
  		} parity;
  		enum { ISO14443A_LAST_BIT_0 = 0, ISO14443A_LAST_BIT_1 = 1, ISO14443A_LAST_BIT_NONE } last_bit;
  		enum { CRC_UNCALCULATED = 2, CRC_OK = 1, CRC_ERROR = 0} crc;
  	} a;
  } parameters;
  u_int32_t numbytes;
  u_int8_t numbits, bit_offset;
  u_int8_t data[MAXIMUM_FRAME_SIZE];
  u_int8_t parity[MAXIMUM_FRAME_SIZE/8+1]; /* parity bit for data[x] is in parity[x/8] & (1<<(x%8)) */
} iso14443_frame;

#endif /*ISO14443_H_*/
