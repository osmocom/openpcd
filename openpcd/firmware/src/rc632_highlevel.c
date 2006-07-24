/* ISO 14443 WUPA / SELECT anticollision implementation, part of OpenPCD 
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
 */

static int
rc632_iso14443a_init(void)
{
	int ret;

	// FIXME: some fifo work (drain fifo?)
	
	/* flush fifo (our way) */
	ret = rc632_reg_write(RC632_REG_CONTROL, 0x01);

	ret = rc632_reg_write(RC632_REG_TX_CONTROL,
			(RC632_TXCTRL_TX1_RF_EN |
			 RC632_TXCTRL_TX2_RF_EN |
			 RC632_TXCTRL_TX2_INV |
			 RC632_TXCTRL_FORCE_100_ASK |
			 RC632_TXCTRL_MOD_SRC_INT));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_CW_CONDUCTANCE,
				CM5121_CW_CONDUCTANCE);
	if (ret < 0)
		return ret;

	/* Since FORCE_100_ASK is set (cf mc073930.pdf), this line may be left out? */
	ret = rc632_reg_write(RC632_REG_MOD_CONDUCTANCE,
				CM5121_MOD_CONDUCTANCE);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_CODER_CONTROL,
				(RC632_CDRCTRL_TXCD_14443A |
				 RC632_CDRCTRL_RATE_106K));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_MOD_WIDTH, 0x13);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_MOD_WIDTH_SOF, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_TYPE_B_FRAMING, 0x00);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_RX_CONTROL1,
			      (RC632_RXCTRL1_GAIN_35DB |
			       RC632_RXCTRL1_ISO14443 |
			       RC632_RXCTRL1_SUBCP_8));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_DECODER_CONTROL,
			      (RC632_DECCTRL_MANCHESTER |
			       RC632_DECCTRL_RXFR_14443A));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_BIT_PHASE,
				CM5121_14443A_BITPHASE);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_RX_THRESHOLD,
				CM5121_14443A_THRESHOLD);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_BPSK_DEM_CONTROL, 0x00);
	if (ret < 0)
		return ret;
			      
	ret = rc632_reg_write(RC632_REG_RX_CONTROL2,
			      (RC632_RXCTRL2_DECSRC_INT |
			       RC632_RXCTRL2_CLK_Q));
	if (ret < 0)
		return ret;

	/* Omnikey proprietary driver has 0x03, but 0x06 is the default reset
	 * value ?!? */
	ret = rc632_reg_write(RC632_REG_RX_WAIT, 0x06);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_CHANNEL_REDUNDANCY,
			      (RC632_CR_PARITY_ENABLE |
			       RC632_CR_PARITY_ODD));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_CRC_PRESET_LSB, 0x63);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(RC632_REG_CRC_PRESET_MSB, 0x63);
	if (ret < 0)
		return ret;

	return 0;
}

static int
rc632_transceive(const u_int8_t *tx_buf,
		 u_int8_t tx_len,
		 u_int8_t *rx_buf,
		 u_int8_t *rx_len,
		 u_int64_t timer,
		 unsigned int toggle)
{
	int ret, cur_tx_len;
	const u_int8_t *cur_tx_buf = tx_buf;

	DEBUGP("timer = %u\n", timer);

	if (tx_len > 64)
		cur_tx_len = 64;
	else
		cur_tx_len = tx_len;

	ret = rc632_timer_set(timer);
	if (ret < 0)
		return ret;
	
	/* clear all interrupts */
	ret = rc632_reg_write(RC632_REG_INTERRUPT_RQ, 0x7f);

	do {	
		ret = rc632_fifo_write(cur_tx_len, cur_tx_buf, 0x03);
		if (ret < 0)
			return ret;

		if (cur_tx_buf == tx_buf) {
			ret = rc632_reg_write(RC632_REG_COMMAND,
					      RC632_CMD_TRANSCEIVE);
			if (ret < 0)
				return ret;
		}

		cur_tx_buf += cur_tx_len;
		if (cur_tx_buf < tx_buf + tx_len) {
			u_int8_t fifo_fill;
			ret = rc632_reg_read(RC632_REG_FIFO_LENGTH,
					     &fifo_fill);
			if (ret < 0)
				return ret;

			cur_tx_len = 64 - fifo_fill;
			printf("refilling tx fifo with %u bytes\n", cur_tx_len);
		} else
			cur_tx_len = 0;

	} while (cur_tx_len);

	if (toggle == 1)
		tcl_toggle_pcb(handle);

	ret = rc632_wait_idle_timer(handle);
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(RC632_REG_FIFO_LENGTH, rx_len);
	if (ret < 0)
		return ret;

	if (*rx_len == 0) {
		u_int8_t tmp;

		DEBUGP("rx_len == 0\n");

		rc632_reg_read(RC632_REG_ERROR_FLAG, &tmp);
		rc632_reg_read(RC632_REG_CHANNEL_REDUNDANCY, &tmp);

		return -1; 
	}

	return rc632_fifo_read(*rx_len, rx_buf);
}

/* issue a 14443-3 A PCD -> PICC command in a short frame, such as REQA, WUPA */
static int
rc632_iso14443a_transceive_sf(u_int8_t cmd,
		    		struct iso14443a_atqa *atqa)
{
	int ret;
	u_int8_t tx_buf[1];
	u_int8_t rx_len = 2;

	memset(atqa, 0, sizeof(atqa));

	tx_buf[0] = cmd;

	/* transfer only 7 bits of last byte in frame */
	ret = rc632_reg_write(RC632_REG_BIT_FRAMING, 0x07);
	if (ret < 0)
		return ret;

	ret = rc632_clear_bits(RC632_REG_CONTROL,
				RC632_CONTROL_CRYPTO1_ON);
	if (ret < 0)
		return ret;

#if 0
	ret = rc632_reg_write(RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD));
#else
	ret = rc632_clear_bits(RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_RX_CRC_ENABLE|RC632_CR_TX_CRC_ENABLE);
				
#endif
	if (ret < 0)
		return ret;

	ret = rc632_transceive(tx_buf, sizeof(tx_buf),
				(u_int8_t *)atqa, &rx_len,
				ISO14443A_FDT_ANTICOL_LAST1, 0);
	if (ret < 0) {
		DEBUGP("error during rc632_transceive()\n");
		return ret;
	}

	/* switch back to normal 8bit last byte */
	ret = rc632_reg_write(RC632_REG_BIT_FRAMING, 0x00);
	if (ret < 0)
		return ret;

	if (rx_len != 2) {
		DEBUGP("rx_len(%d) != 2\n", rx_len);
		return -1;
	}

	return 0;
}

/* transceive regular frame */
static int
rc632_iso14443ab_transceive(struct rfid_asic_handle *handle,
			   unsigned int frametype,
			   const u_int8_t *tx_buf, unsigned int tx_len,
			   u_int8_t *rx_buf, unsigned int *rx_len,
			   u_int64_t timeout, unsigned int flags)
{
	int ret;
	u_int8_t rxl = *rx_len & 0xff;
	u_int8_t channel_red;

	memset(rx_buf, 0, *rx_len);

	switch (frametype) {
	case RFID_14443A_FRAME_REGULAR:
	case RFID_MIFARE_FRAME:
		channel_red = RC632_CR_RX_CRC_ENABLE|RC632_CR_TX_CRC_ENABLE
				|RC632_CR_PARITY_ENABLE|RC632_CR_PARITY_ODD;
		break;
	case RFID_14443B_FRAME_REGULAR:
		channel_red = RC632_CR_RX_CRC_ENABLE|RC632_CR_TX_CRC_ENABLE
				|RC632_CR_CRC3309;
		break;
#if 0
	case RFID_MIFARE_FRAME:
		channel_red = RC632_CR_PARITY_ENABLE|RC632_CR_PARITY_ODD;
		break;
#endif
	default:
		return -EINVAL;
		break;
	}
	ret = rc632_reg_write(RC632_REG_CHANNEL_REDUNDANCY,
			      channel_red);
	if (ret < 0)
		return ret;

	ret = rc632_transceive(tx_buf, tx_len, rx_buf, &rxl, timeout, 0);
	*rx_len = rxl;
	if (ret < 0)
		return ret;


	return 0; 
}

/* transceive anti collission bitframe */
static int
rc632_iso14443a_transceive_acf(struct iso14443a_anticol_cmd *acf,
			       unsigned int *bit_of_col)
{
	int ret;
	u_int8_t rx_buf[64];
	u_int8_t rx_len = sizeof(rx_buf);
	u_int8_t rx_align = 0, tx_last_bits, tx_bytes;
	u_int8_t boc;
	u_int8_t error_flag;
	*bit_of_col = ISO14443A_BITOFCOL_NONE;
	memset(rx_buf, 0, sizeof(rx_buf));

	/* disable mifare cryto */
	ret = rc632_clear_bits(RC632_REG_CONTROL,
				RC632_CONTROL_CRYPTO1_ON);
	if (ret < 0)
		return ret;

	/* disable CRC summing */
#if 0
	ret = rc632_reg_write(RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD));
#else
	ret = rc632_clear_bits(RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_TX_CRC_ENABLE|RC632_CR_TX_CRC_ENABLE);
#endif
	if (ret < 0)
		return ret;

	tx_last_bits = acf->nvb & 0x0f;	/* lower nibble indicates bits */
	tx_bytes = acf->nvb >> 4;
	if (tx_last_bits) {
		tx_bytes++;
		rx_align = (tx_last_bits+1) % 8;/* rx frame complements tx */
	}

	//rx_align = 8 - tx_last_bits;/* rx frame complements tx */

	/* set RxAlign and TxLastBits*/
	ret = rc632_reg_write(RC632_REG_BIT_FRAMING,
				(rx_align << 4) | (tx_last_bits));
	if (ret < 0)
		return ret;

	ret = rc632_transceive((u_int8_t *)acf, tx_bytes,
				rx_buf, &rx_len, 0x32, 0);
	if (ret < 0)
		return ret;

	/* bitwise-OR the two halves of the split byte */
	acf->uid_bits[tx_bytes-2] = (
		  (acf->uid_bits[tx_bytes-2] & (0xff >> (8-tx_last_bits)))
		| rx_buf[0]);
	/* copy the rest */
	memcpy(&acf->uid_bits[tx_bytes+1-2], &rx_buf[1], rx_len-1);

	/* determine whether there was a collission */
	ret = rc632_reg_read(RC632_REG_ERROR_FLAG, &error_flag);
	if (ret < 0)
		return ret;

	if (error_flag & RC632_ERR_FLAG_COL_ERR) {
		/* retrieve bit of collission */
		ret = rc632_reg_read(RC632_REG_COLL_POS, &boc);
		if (ret < 0)
			return ret;

		/* bit of collission relative to start of part 1 of 
		 * anticollision frame (!) */
		*bit_of_col = 2*8 + boc;
	}

	return 0;
}

#if 0
enum rc632_rate {
	RC632_RATE_106	= 0x00,
	RC632_RATE_212	= 0x01,
	RC632_RATE_424	= 0x02,
	RC632_RATE_848	= 0x03,
};

struct rx_config {
	u_int8_t	subc_pulses;
	u_int8_t	rx_coding;
	u_int8_t	rx_threshold;
	u_int8_t	bpsk_dem_ctrl;
};

struct tx_config {
	u_int8_t	rate;
	u_int8_t	mod_width;
};

static struct rx_config rx_configs[] = {
	{
		.subc_pulses 	= RC632_RXCTRL1_SUBCP_8,
		.rx_coding	= RC632_DECCTRL_MANCHESTER,
		.rx_threshold	= 0x88,
		.bpsk_dem_ctrl	= 0x00,
	},
	{
		.subc_pulses	= RC632_RXCTRL1_SUBCP_4,
		.rx_coding	= RC632_DECCTRL_BPSK,
		.rx_threshold	= 0x50,
		.bpsk_dem_ctrl	= 0x0c,
	},
	{
		.subc_pulses	= RC632_RXCTRL1_SUBCP_2,
		.rx_coding	= RC632_DECCTRL_BPSK,
		.rx_threshold	= 0x50,
		.bpsk_dem_ctrl	= 0x0c,
	},
	{
		.subc_pulses	= RC632_RXCTRL1_SUBCP_1,
		.rx_coding	= RC632_DECCTRL_BPSK,
		.rx_threshold	= 0x50,
		.bpsk_dem_ctrl	= 0x0c,
	},
};

static struct tx_config tx_configs[] = {
	{
		.rate 		= RC632_CDRCTRL_RATE_106K,
		.mod_width	= 0x13,
	},
	{
		.rate		= RC632_CDRCTRL_RATE_212K,
		.mod_width	= 0x07,
	},
	{
		.rate		= RC632_CDRCTRL_RATE_424K,
		.mod_width	= 0x03,
	},
	{
		.rate		= RC632_CDRCTRL_RATE_848K,
		.mod_width	= 0x01,
	},
};

static int rc632_iso14443a_set_speed(struct rfid_asic_handle *handle,
				     unsigned int tx,
				     u_int8_t rate)
{
	int rc;
	u_int8_t reg;


	if (!tx) {
		/* Rx */
		if (rate > ARRAY_SIZE(rx_configs))
			return -EINVAL;

		rc = rc632_set_bit_mask(RC632_REG_RX_CONTROL1,
					RC632_RXCTRL1_SUBCP_MASK,
					rx_configs[rate].subc_pulses);
		if (rc < 0)
			return rc;

		rc = rc632_set_bit_mask(RC632_REG_DECODER_CONTROL,
					RC632_DECCTRL_BPSK,
					rx_configs[rate].rx_coding);
		if (rc < 0)
			return rc;

		rc = rc632_reg_write(RC632_REG_RX_THRESHOLD,
					rx_configs[rate].rx_threshold);
		if (rc < 0)
			return rc;

		if (rx_configs[rate].rx_coding == RC632_DECCTRL_BPSK) {
			rc = rc632_reg_write(
					     RC632_REG_BPSK_DEM_CONTROL,
					     rx_configs[rate].bpsk_dem_ctrl);
			if (rc < 0)
				return rc;
		}
	} else {
		/* Tx */
		if (rate > ARRAY_SIZE(tx_configs))
			return -EINVAL;

		rc = rc632_set_bit_mask(RC632_REG_CODER_CONTROL,
					RC632_CDRCTRL_RATE_MASK,
					tx_configs[rate].rate);
		if (rc < 0)
			return rc;

		rc = rc632_reg_write(RC632_REG_MOD_WIDTH,
				     tx_configs[rate].mod_width);
		if (rc < 0)
			return rc;
	}

	return 0;
}
#endif
