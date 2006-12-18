#ifndef _OPENPICC_H
#define _OPENPICC_H

/* OpenPICC Register definition
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 */

enum openpicc_register {
	OPICC_REG_MODE,			/* operational mode */
	OPICC_REG_ISO14443A_FDT_0,	/* FDT (after 0) in carrier cycles */
	OPICC_REG_ISO14443A_FDT_1,	/* FDT (after 1) in carrier cycles */
	OPICC_REG_BITCLK_PHASE_CORR,	/* signed 8bit phase correction */
	OPICC_REG_SPEED_RX,
	OPICC_REG_SPEED_TX,
	OPICC_REG_UID_PUPI,		/* UID (14443A) / PUPI (14443B) */
};

enum openpicc_reg_mode {
	OPICC_MODE_14443A,
	OPICC_MODE_14443B,
	OPICC_MODE_LOWLEVEL,		/* low-level bit-transceive mode TBD */
};

enum openpicc_reg_speed {
	OPICC_SPEED_14443_106K,
	OPICC_SPEED_14443_212K,
	OPICC_SPEED_14443_424K,
	OPICC_SPEED_14443_848K,
};

#endif /* _OPENPICC_H */
