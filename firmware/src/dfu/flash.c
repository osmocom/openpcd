#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include <dfu/dbgu.h>
#include <board.h>

#define EFCS_CMD_WRITE_PAGE		0x1
#define EFCS_CMD_SET_LOCK_BIT		0x2
#define EFCS_CMD_WRITE_PAGE_LOCK	0x3
#define EFCS_CMD_CLEAR_LOCK		0x4
#define EFCS_CMD_ERASE_ALL		0x8
#define EFCS_CMD_SET_NVM_BIT		0xb
#define EFCS_CMD_CLEAR_NVM_BIT		0xd
#define EFCS_CMD_SET_SECURITY_BIT	0xf


static u_int16_t page_from_ramaddr(const void *addr)
{
	u_int32_t ramaddr = (u_int32_t) addr;
	ramaddr -= (u_int32_t) AT91C_IFLASH;
	return ((ramaddr >> AT91C_IFLASH_PAGE_SHIFT));
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
	page &= 0x3ff;
	AT91F_MC_EFC_PerformCmd(AT91C_BASE_MC, AT91C_MC_FCMD_UNLOCK |
				AT91C_MC_CORRECT_KEY | (page << 8));
}

void flash_page(u_int8_t *addr)
{
	u_int16_t page = page_from_ramaddr(addr) & 0x3ff;
	u_int32_t fsr = AT91F_MC_EFC_GetStatus(AT91C_BASE_MC);
	DEBUGP("flash_page(0x%x=%u) ", addr, page);

	if (is_page_locked(page)) {
		DEBUGP("unlocking ");
		unlock_page(page);
	}

	if (!(fsr & AT91C_MC_FRDY)) {
		DEBUGP("NOT_FLASHING ");		
		return;
	}

	DEBUGP("performing start_prog ");
#if 1
	AT91F_MC_EFC_PerformCmd(AT91C_BASE_MC, AT91C_MC_FCMD_START_PROG |
				AT91C_MC_CORRECT_KEY | (page << 8));
#endif
}

void flash_init(void)
{
	unsigned int fmcn = AT91F_MC_EFC_ComputeFMCN(MCK);

	AT91F_MC_EFC_CfgModeReg(AT91C_BASE_MC, fmcn << 16 | AT91C_MC_FWS_3FWS |
				AT91C_MC_FRDY | AT91C_MC_LOCKE | 
				AT91C_MC_PROGE);
}
