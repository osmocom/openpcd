#ifndef _SSC_H
#define _SSC_H

#include "queue.h"

extern void ssc_rx_start(void);
extern void ssc_rx_stop(void);

/* Rx/Tx initialization separate, since Tx disables PWM output ! */
extern void ssc_tx_init(void);
extern void ssc_rx_init(void);

extern void ssc_fini(void);
extern void ssc_rx_stop(void);
extern void ssc_rx_unthrottle(void);

enum ssc_mode {
	SSC_MODE_NONE,
	SSC_MODE_14443A_SHORT,
	SSC_MODE_14443A_STANDARD,
	SSC_MODE_14443B,
	SSC_MODE_EDGE_ONE_SHOT,
	SSC_MODE_CONTINUOUS,
};

extern void ssc_rx_mode_set(enum ssc_mode ssc_mode);

extern portBASE_TYPE ssc_get_overflows(void);
extern int ssc_count_free(void);

#define SSC_DMA_BUFFER_SIZE 2048
#define SSC_DMA_BUFFER_COUNT 10

typedef enum {
	FREE=0,    /* Buffer is free */
	PENDING,   /* Buffer has been given to the DMA controller and is currently being filled */
	FULL,      /* DMA controller signalled that the buffer is full */
	PROCESSING /* The buffer is currently processed by the consumer (e.g. decoder) */
} ssc_dma_buffer_state_t;

typedef struct {
	volatile ssc_dma_buffer_state_t state;
	u_int32_t len;  /* Length of the content */
	u_int8_t data[SSC_DMA_BUFFER_SIZE];
} ssc_dma_buffer_t;

extern xQueueHandle ssc_rx_queue;

#endif
