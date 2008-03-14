#ifndef TC_RECV_H_
#define TC_RECV_H_

#include "iso14443.h"

#define TC_RECV_NUMBER_OF_FRAME_BUFFERS 10

struct tc_recv_handle;
typedef struct tc_recv_handle *tc_recv_handle_t;

typedef enum {
	TC_RECV_CALLBACK_RX_FRAME_ENDED,    // *data is iso14443_frame *frame
	TC_RECV_CALLBACK_SETUP,             // *data is tc_recv_handle_t th
	TC_RECV_CALLBACK_TEARDOWN,          // *data is tc_recv_handle_t th
} tc_recv_callback_reason;
typedef void (*tc_recv_callback_t)(tc_recv_callback_reason reason, void *data);

extern int tc_recv_init(tc_recv_handle_t *th, int pauses_count, tc_recv_callback_t callback);
extern int tc_recv_receive(tc_recv_handle_t th, iso14443_frame* *frame, unsigned int timeout);

#endif /*TC_RECV_H_*/
