#ifndef _SSC_H
#define _SSC_H

extern void ssc_rx_start(void);
extern void ssc_rx_stop(void);

/* Rx/Tx initialization separate, since Tx disables PWM output ! */
extern void ssc_tx_init(void);
extern void ssc_rx_init(void);

extern void ssc_fini(void);
extern void ssc_rx_stop(void);
extern void ssc_rx_unthrottle(void);

#endif
