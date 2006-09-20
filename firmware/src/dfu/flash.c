#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>

#define EFCS_CMD_WRITE_PAGE		0x01
#define EFCS_CMD_SET_LOCK_BIT		0x02
#define EFCS_CMD_WRITE_PAGE_LOCK	0x03
#define EFCS_CMD_CLEAR_LOCK		0x04
#define EFCS_CMD_ERASE_ALL		0x08
#define EFCS_CMD_SET_NVM_BIT		0x0b
#define EFCS_CMD_CLEAR_NVM_BIT		0x0d
#define EFCS_CMD_SET_SECURITY_BIT	0x0f


static u_int16_t page_from_ramaddr(const void *addr)
{
	u_int32_t ramaddr = (u_int32_t) addr;
	ramaddr -= (u_int32_t) AT91C_IFLASH;
	return (ramaddr >> AT91C_IFLASH_PAGE_SHIFT);
}
#define PAGES_PER_LOCKREGION	(AT91C_IFLASH_LOCK_REGION_SIZE>>AT91C_IFLASH_PAGE_SHIFT)
#define IS_FIRST_PAGE_OF_LOCKREGION(x)	((x % PAGES_PER_LOCKREGION) == 0)
#define LOCKREGION_FROM_PAGE(x)	(x / PAGES_PER_LOCKREGION)

static int is_page_locked(u_int16_t page)
{
	u_int16_t lockregion = LOCKREGION_FROM_PAGE(page);

	return (AT91C_BASE_MC->MC_FSR & (lockregion << 16));
}

static void unlock_page(u_int16_t page)
{
	AT91F_MC_EFC_PerformCmd(AT91C_BASE_MC, AT91C_MC_FCMD_UNLOCK |
				AT91C_MC_CORRECT_KEY | page);
}

void flash_page(u_int8_t *addr)
{
	u_int16_t page = page_from_ramaddr(addr);

	if (is_page_locked(page))
		unlock_page(page);

	AT91F_MC_EFC_PerformCmd(AT91C_BASE_MC, AT91C_MC_FCMD_START_PROG |
				AT91C_MC_CORRECT_KEY | page);
}


