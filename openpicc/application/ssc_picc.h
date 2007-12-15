#ifndef _SSC_H
#define _SSC_H

#include "queue.h"
#include "iso14443_layer3a.h"

extern void ssc_rx_start(void);
extern void ssc_rx_stop(void);

/* Rx/Tx initialization separate, since Tx disables PWM output ! */
extern void ssc_tx_init(void);
extern void ssc_rx_init(void);

extern void ssc_fini(void);
extern void ssc_rx_stop(void);
extern void ssc_rx_unthrottle(void);
extern void __ramfunc ssc_rx_stop_frame_ended(void);


enum ssc_mode {
	SSC_MODE_NONE,
	SSC_MODE_14443A_SHORT,
	SSC_MODE_14443A_STANDARD,
	SSC_MODE_14443A,
	SSC_MODE_14443B,
	SSC_MODE_EDGE_ONE_SHOT,
	SSC_MODE_CONTINUOUS,
};

extern void ssc_rx_mode_set(enum ssc_mode ssc_mode);

typedef void (*ssc_irq_ext_t)(u_int32_t ssc_sr, enum ssc_mode ssc_mode, u_int8_t* samples);

/* A fast method to extend the IRQ handler from the higher level code, e.g. to prepare
 * an ATQA answer to REQA or WUPA in iso14443_layer3a. Normally I'd use the FreeRTOS 
 * primitives to wake the task and then do everything in task context, but the delay
 * from SSC IRQ to the task returning from xQueueReceive is around 165us. Additionally to the
 * delay from end of communication to SSC IRQ which is around 50us. This results in way more delay
 * than acceptable for the synchronous responses (around 87us).*/ 
extern ssc_irq_ext_t ssc_set_irq_extension(ssc_irq_ext_t ext_handler);

/* These are various SSC performance metrics that can be queried */
typedef enum {
	OVERFLOWS,     /* Overflows (e.g. no free buffer when reloading DMA controller) */
	BUFFER_ERRORS, /* Internal buffer management errors */
	FREE_BUFFERS,  /* Free RX buffers */
	LATE_FRAMES,   /* Frames that were not ready to be sent when the FDT passed; e.g. sent too late */
	SSC_ERRORS,    /* General error count, e.g. OVERFLOWS + BUFFER_ERRORS */
} ssc_metric;
extern int ssc_get_metric(ssc_metric metric);

#define SSC_RX_BUFFER_SIZE 2048
#define SSC_DMA_BUFFER_COUNT 4

#if SSC_RX_BUFFER_SIZE < ISO14443A_MAX_RX_FRAME_SIZE_IN_BITS
#undef SSC_RX_BUFFER_SIZE
#define SSC_RX_BUFFER_SIZE ISO14443A_MAX_RX_FRAME_SIZE_IN_BITS
#endif

typedef enum {
	FREE=0,    /* Buffer is free */
	PENDING,   /* Buffer has been given to the DMA controller and is currently being filled */
	FULL,      /* DMA controller signalled that the buffer is full */
	PROCESSING,/* The buffer is currently processed by the consumer (e.g. decoder) */
	PREFILLED, /* The buffer has been prefilled for later usage (only used for TX) */
} ssc_dma_buffer_state_t;

typedef struct {
	enum ssc_mode mode;
	u_int16_t transfersize_ssc;
	u_int16_t transfersize_pdc;
	u_int16_t transfers;
} ssc_mode_def;

typedef struct {
	volatile ssc_dma_buffer_state_t state;
	u_int32_t len;  /* Length of the content, in samples */
	const ssc_mode_def *reception_mode; /* Pointer to the SSC mode definition that the buffer has been loaded for (affects element size and count) */
	u_int8_t data[SSC_RX_BUFFER_SIZE];
} ssc_dma_rx_buffer_t;

extern xQueueHandle ssc_rx_queue;

/* in bytes, used for the sample buffer that holds the subcarrier modulation data at fc/8 = 1695 MHz */
#define SSC_TX_BUFFER_SIZE ((MAXIMUM_FRAME_SIZE*( (8+1)*2 ) ) + 2 + 2)

typedef struct {
	volatile ssc_dma_buffer_state_t state;
	u_int32_t len;  /* Length of the content */
	void *source; /* Source pointer for a prefilled buffer; set to NULL if not used */
	u_int8_t data[SSC_TX_BUFFER_SIZE];
} ssc_dma_tx_buffer_t;

/* Declare one TX buffer. This means that only one buffer can ever be pending for sending. That's because
 * this buffer must be huge (one frame of 256 bytes of subcarrier modulation data at  1695 MHz sample
 * rate is approx 4k bytes). */
extern ssc_dma_tx_buffer_t ssc_tx_buffer;

extern void ssc_tx_start(ssc_dma_tx_buffer_t *buf);
extern volatile u_int32_t ssc_tx_fiq_fdt_cdiv;
extern volatile u_int32_t ssc_tx_fiq_fdt_ssc;

#endif
