#ifndef _RC623_API_H
#define _RC632_API_H

#include <sys/types.h>
#include <cl_rc632.h>
#include <librfid/rfid.h>
#include <librfid/rfid_asic.h>

extern int opcd_rc632_reg_write(struct rfid_asic_handle *hdl,
				u_int8_t addr, u_int8_t data);
extern int opcd_rc632_fifo_write(struct rfid_asic_handle *hdl,
				 u_int8_t len, u_int8_t *data, u_int8_t flags);
extern int opcd_rc632_reg_read(struct rfid_asic_handle *hdl,
			       u_int8_t addr, u_int8_t *val);
extern int opcd_rc632_fifo_read(struct rfid_asic_handle *hdl,
				u_int8_t max_len, u_int8_t *data);
extern int opcd_rc632_clear_bits(struct rfid_asic_handle *hdl,
				 u_int8_t reg, u_int8_t bits);
extern int opcd_rc632_set_bits(struct rfid_asic_handle *hdl,
				u_int8_t reg, u_int8_t bits);

extern void rc632_init(void);
extern void rc632_exit(void);

extern void rc632_unthrottle(void);

#ifdef DEBUG
extern int rc632_test(struct rfid_asic_handle *hdl);
extern int rc632_dump(void);
#endif

#endif
