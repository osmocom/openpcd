#ifndef _RC623_API_H
#define _RC632_API_H

#include <sys/types.h>
#include <cl_rc632.h>
#include <librfid/rfid.h>
#include <librfid/rfid_asic.h>

extern int opcd_rc632_reg_write(struct rfid_asic_handle *hdl,
				uint8_t addr, uint8_t data);
extern int opcd_rc632_fifo_write(struct rfid_asic_handle *hdl,
				 uint8_t len, uint8_t *data, uint8_t flags);
extern int opcd_rc632_reg_read(struct rfid_asic_handle *hdl,
			       uint8_t addr, uint8_t *val);
extern int opcd_rc632_fifo_read(struct rfid_asic_handle *hdl,
				uint8_t max_len, uint8_t *data);
extern int opcd_rc632_clear_bits(struct rfid_asic_handle *hdl,
				 uint8_t reg, uint8_t bits);
extern int opcd_rc632_set_bits(struct rfid_asic_handle *hdl,
				uint8_t reg, uint8_t bits);

extern void rc632_init(void);
extern void rc632_exit(void);

extern void rc632_unthrottle(void);

extern int rc632_test(struct rfid_asic_handle *hdl);
extern int rc632_dump(void);

extern void rc632_power(uint8_t up);

#endif
