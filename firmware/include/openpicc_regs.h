#ifndef _OPENPICC_STATE
#define _OPENPICC_STATE

/* according to ISO 14443-3(2000) 6.2 */
enum opicc_14443a_state {
	ISO14443A_ST_POWEROFF,
	ISO14443A_ST_IDLE,
	ISO14443A_ST_READY,
	ISO14443A_ST_ACTIVE,
	ISO14443A_ST_HALT,
	ISO14443A_ST_READY2,
	ISO14443A_ST_ACTIVE2,
};

enum opicc_reg_tx_control {
	OPICC_REG_TX_BPSK	= 0x01,
	OPICC_REG_TX_MANCHESTER	= 0x02,
	OPICC_REG_TX_INTENSITY0	= 0x00,
	OPICC_REG_TX_INTENSITY1	= 0x10,
	OPICC_REG_TX_INTENSITY2	= 0x20,
	OPICC_REG_TX_INTENSITY3	= 0x30,
};

enum opicc_reg {
	OPICC_REG_14443A_UIDLEN,/* Length of UID in bytes */

	OPICC_REG_14443A_FDT0,	/* Frame delay time if last bit 0 */
	OPICC_REG_14443A_FDT1,	/* Frame delay time if last bit 1 */
	OPICC_REG_14443A_STATE,	/* see 'enum opicc_14443a_state' */
	OPICC_REG_14443A_ATQA,	/* The ATQA template for 14443A */

	OPICC_REG_RX_CLK_DIV,	/* Clock divider for Rx sample clock */
	OPICC_REG_RX_CLK_PHASE,	/* Phase shift of Rx sample clock */
	OPICC_REG_RX_COMP_LEVEL,/* Comparator level of Demodulator */
	OPICC_REG_RX_CONTROL,

	OPICC_REG_TX_CLK_DIV,	/* Clock divider for Tx sample clock */
	OPICC_REG_TX_CONTROL,	/* see 'enum opicc_reg_tx_Control */
	_OPICC_NUM_REGS,
};

enum openpicc_14443a_sregs {
	/* string 'registers' */
	OPICC_REG_14443A_UID,	/* The UID (4...10 bytes) */
};

#endif
