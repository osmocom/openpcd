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

enum ssc_mode {
	SSC_MODE_NONE,
	SSC_MODE_14443A_SHORT,
	SSC_MODE_14443A_STANDARD,
	SSC_MODE_14443B,
	SSC_MODE_EDGE_ONE_SHOT,
	SSC_MODE_CONTINUOUS,
};

extern void ssc_rx_mode_set(enum ssc_mode ssc_mode);

#endif
