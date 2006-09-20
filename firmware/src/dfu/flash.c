

#define EFCS_CMD_WRITE_PAGE		0x01
#define EFCS_CMD_SET_LOCK_BIT		0x02
#define EFCS_CMD_WRITE_PAGE_LOCK	0x03
#define EFCS_CMD_CLEAR_LOCK		0x04
#define EFCS_CMD_ERASE_ALL		0x08
#define EFCS_CMD_SET_NVM_BIT		0x0b
#define EFCS_CMD_CLEAR_NVM_BIT		0x0d
#define EFCS_CMD_SET_SECURITY_BIT	0x0f


int unlock_page(u_int16_t page)
{
	AT91C_MC_FCMD_UNLOCK | AT91C_MC_CORRECT_KEY | 

}

int flash_sector(unsigned int sector, const u_int8_t *data, unsigned int len)
{
	volatile u_int32_t *p = (volatile u_int32_t *)0;
	u_int32_t *src32 = (u_int32_t *)data;
	int i;

	/* hand-code memcpy because we need to make sure only 32bit accesses
	 * are used */
	for (i = 0; i < len/4; i++)
		p[i] = src32[i];

	AT91F_MC_EFC_PerformCmd(pmc , AT91C_MC_FCMD_START_PROG|
				AT91C_MC_CORRECT_KEY | );
}



