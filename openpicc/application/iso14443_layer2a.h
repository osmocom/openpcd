#ifndef ISO14443_LAYER2A_H_
#define ISO14443_LAYER2A_H_

#include "ssc_buffer.h"

/* Callback type for iso14443_receive().
 * Parameter buffer is being passed a pointer to an SSC Rx buffer structure that this receive happened
 * on. You might want to pass this into iso14443a_decode_miller().
 * Parameter in_irq is true if the callback has been called while still in IRQ mode. It must then *NOT*
 * perform any calls that might upset the IRQ processing. Especially it may not call into FreeRTOS or
 * any parts of the applications that do.
 */
typedef void (*iso14443_receive_callback_t)(ssc_dma_rx_buffer_t *buffer, u_int8_t in_irq);

/* Wait for and receive a frame. Parameters callback and buffer are optional. If you omit them you'll lose
 * the received frame, obviously.
 * Parameter callback is a callback function that will be called for each received frame and might
 * then trigger a response.
 * Parameter buffer is an output pointer to a pointer to an SSC Rx buffer structure containing the
 * received frame.
 * Parameter timeout gives an optional timeout for the receive operation after which the receive will
 * be aborted. When timeout is 0 the receive will not time out.
 * This call will block until a frame is received or an exception happens. Obviously it must not be run
 * from IRQ context.
 * 
 * Warning: When you get a buffer from the function then its state is set to PROCESSING and you must 
 * FREE it yourself. However, you MUST NOT free a buffer from the callback.
 * 
 * Return values:
 * >= 0        Frame received, return value is buffer length (yes, 0 is a valid buffer length)
 * -ENETDOWN   PLL is not locked or PLL lock lost
 * -ETIMEDOUT  Receive timed out without receiving anything (usually not regarded an error condition)
 * -EBUSY      A Tx is currently running or pending; can't receive
 * -EALREADY   There's already an iso14443_receive() invocation running
 */
extern int iso14443_receive(iso14443_receive_callback_t callback, ssc_dma_rx_buffer_t **buffer, unsigned int timeout);

/*
 * Transmit a frame. Starts transmitting fdt carrier cycles after the end of the received frame.
 * Parameters buffer and fdt specify the SSC Tx buffer to be sent and the frame delay time in carrier cycles.
 * When parameter async is set then this call will only schedule the buffer for sending and return immediately,
 * otherwise the call will block until the buffer has been sent completely, until the specified timeout is reached
 * or an error occurs.
 * This function may be run from IRQ context, especially from within the iso14443_receive callback, with the async
 * parameter set.
 * 
 * Note: When your callback calls iso14443_transmit() from IRQ context it must set the async parameter to have transmit
 * return immediately and then not block within the callback. When the scheduled transmission has not happened by the next
 * time you call iso14443_receive() it will abort with -EBUSY and you can then either decide to wait for the transmission
 * to go through, or abort the scheduled transmission with iso14443_tx_abort(). You can check for whether a Tx is currently
 * pending or running with iso14443_tx_busy().
 */
extern int iso14443_transmit(ssc_dma_tx_buffer_t *buffer, unsigned int fdt, u_int8_t async, unsigned int timeout);

extern int iso14443_tx_abort(void);
extern int iso14443_tx_busy(void);

/*
 * Wait for the presence of a reader to be detected.
 * Returns 0 when the PLL is locked.
 */
extern int iso14443_wait_for_carrier(unsigned int timeout);

/* Initialize the layer 2 transceiver code.
 * Parameter enable_fast_receive, when set, will enable special code to guarantee fast response times
 * for frames shorter than or equal to 56 data bits (bit oriented anticollision frame length). This
 * generally involves running the callback from an IRQ handler. 
 */
extern int iso14443_layer2a_init(u_int8_t enable_fast_receive);

extern u_int8_t iso14443_set_fast_receive(u_int8_t enable_fast_receive);
extern u_int8_t iso14443_get_fast_receive(void);

#endif /*ISO14443_LAYER2A_H_*/
