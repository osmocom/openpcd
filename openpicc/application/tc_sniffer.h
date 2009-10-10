#ifndef TC_SNIFFER_H_
#define TC_SNIFFER_H_

#define TC_FIQ_BUFSIZE 1024
typedef struct {
	u_int32_t count;
	u_int32_t data[TC_FIQ_BUFSIZE];
} tc_fiq_buffer_t;
typedef void tc_fiq_buffer_handler_t(tc_fiq_buffer_t *buffer);

extern void tc_sniffer (void *pvParameters);
extern void tc_fiq_setup(void);
extern void tc_fiq_start(void);
extern void tc_fiq_stop(void);
extern void tc_fiq_process(tc_fiq_buffer_handler_t *handler);
extern void start_stop_sniffing (void);

#endif /*TC_SNIFFER_H_*/
