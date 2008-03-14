#ifndef SSC_H_
#define SSC_H_

#include "board.h"
#include "ssc_buffer.h"

typedef enum { 
	METRIC_RX_OVERFLOWS,      // No free buffer during Rx reload
	METRIC_FREE_RX_BUFFERS,   // Free Rx buffers, calculated on-the-fly
	METRIC_MANAGEMENT_ERRORS, // One of the diverse types of management errors
	METRIC_MANAGEMENT_ERRORS_1, // Internal buffer management error type 1
	METRIC_MANAGEMENT_ERRORS_2, // Internal buffer management error type 2
	METRIC_MANAGEMENT_ERRORS_3, // Internal buffer management error type 3
	METRIC_LATE_TX_FRAMES,    // Frames that only reached the SSC when TF already was high
	METRIC_RX_FRAMES,         // Frames received
	METRIC_TX_FRAMES,         // Frames sent
	METRIC_TX_ABORTED_FRAMES, // Aborted send frames
	_MAX_METRICS,
} ssc_metric;

extern int ssc_get_metric(ssc_metric metric, char **description, int *value);

typedef enum {
	SSC_CALLBACK_RX_STARTING,       // *data is ssh_handle_t *sh
	SSC_CALLBACK_RX_STOPPED,        // *data is ssh_handle_t *sh
	SSC_CALLBACK_RX_FRAME_BEGIN,    // *data is int *end_asserted
									//  may set *end_asserted = 1 to force tell the IRQ handler  
									//  that you have detected the end of reception
	SSC_CALLBACK_RX_FRAME_ENDED,    // *data is ssc_dma_rx_buffer *buffer
	SSC_CALLBACK_TX_FRAME_BEGIN,
	SSC_CALLBACK_TX_FRAME_ENDED,
	SSC_CALLBACK_TX_FRAME_ABORTED,
	SSC_CALLBACK_SETUP,             // *data is ssh_handle_t *sh
	SSC_CALLBACK_TEARDOWN,          // *data is ssh_handle_t *sh
} ssc_callback_reason;
typedef void (*ssc_callback_t)(ssc_callback_reason reason, void *data);

struct _ssc_handle;

typedef struct _ssc_handle ssc_handle_t;

extern void ssc_set_gate(int data_enabled);

extern void ssc_frame_started(void);

/* Rx/Tx initialization separate, since Tx disables PWM output ! */
extern ssc_handle_t* ssc_open(u_int8_t init_rx, u_int8_t init_tx, enum ssc_mode mode , ssc_callback_t callback);

extern int ssc_recv(ssc_handle_t* sh, ssc_dma_rx_buffer_t* *buffer, unsigned int timeout);
extern int ssc_send(ssc_handle_t* sh, ssc_dma_tx_buffer_t* buffer);
extern int ssc_send_abort(ssc_handle_t* sh);

extern void ssc_hard_reset(ssc_handle_t *sh);

extern int ssc_close(ssc_handle_t* sh);

extern void ssc_rx_stop_frame_ended(void);

#endif /*SSC_H_*/
