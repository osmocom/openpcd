#ifndef SSC_H_
#define SSC_H_

extern unsigned long ssc_get_buffers_empty(void);
extern unsigned long ssc_get_buffers_processed(void);
extern unsigned long ssc_get_irq_count(void);
extern void ssc_start_stop_sending_buffers(void);
extern int ssc_init(void);

#endif /*SSC_H_*/
