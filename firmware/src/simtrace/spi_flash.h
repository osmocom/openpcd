#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#include <sys/types.h>

#define OTP_ADDR(x)	(0x114 + ( ((x) - 1) * 16 ) )

void spiflash_init(void);
void spiflash_get_id(u_int8_t *id);
int spiflash_read_status(void);
void spiflash_clear_status(void);
void spiflash_write_protect(int on);
int spiflash_write_enable(int enable);
int spiflash_otp_read(u_int32_t otp_addr, u_int8_t *out, u_int16_t rx_len);
int spiflash_otp_write(u_int32_t otp_addr, u_int8_t data);
int spiflash_otp_get_lock(u_int8_t region);
int spiflash_otp_set_lock(u_int8_t region);

#endif
