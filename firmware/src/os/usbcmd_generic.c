/* Some generel USB API commands, common between OpenPCD and OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 */

#include <string.h>
#include <sys/types.h>
#include <openpcd.h>
#include <os/req_ctx.h>
#include <os/usb_handler.h>
#include <os/led.h>
#include <os/dbgu.h>
#include <os/main.h>
#include <os/flash.h>
#include <board.h>
#ifdef  PCD
#include <rc632_highlevel.h>
#endif/*PCD*/

#define OPENPCD_API_VERSION (0x01)
#define CONFIG_AREA_ADDR ((void*)(AT91C_IFLASH + AT91C_IFLASH_SIZE - ENVIRONMENT_SIZE))
#define CONFIG_AREA_WORDS ( AT91C_IFLASH_PAGE_SIZE/sizeof(u_int32_t) )

volatile u_int32_t config_stack[ CONFIG_AREA_WORDS ];

static int gen_setenv(const void* buffer,int len)
{
    volatile unsigned int i;
    u_int32_t *dst;

    if( len >= sizeof(config_stack) )
	len=sizeof(config_stack);
	
    memcpy(&config_stack,buffer,len);
    
    /* retrieve current content to allow partial flashing */    
    
    /* flash changes */
    dst=(u_int32_t*)CONFIG_AREA_ADDR;
    for(i=0;i<CONFIG_AREA_WORDS;i++)
	*dst++=config_stack[i];

    flash_page(CONFIG_AREA_ADDR);
	
    return len;
}

static int gen_getenv(void* buffer,int len)
{
    if( len >= sizeof(config_stack) )
	len=sizeof(config_stack);

    memcpy(buffer,&config_stack,len);

    return len;
}

static int gen_usb_rx(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) rctx->data;
	struct openpcd_compile_version *ver =
	    (struct openpcd_compile_version *)poh->data; 
	u_int32_t len = rctx->tot_len-sizeof(*poh);

        /* initialize transmit length to header length */
        rctx->tot_len = sizeof(*poh);

	switch (poh->cmd) {

	case OPENPCD_CMD_GET_API_VERSION:
		DEBUGP("CMD_GET_API_VERSION\n");
		poh->flags &= OPENPCD_FLAG_RESPOND;
		poh->val = OPENPCD_API_VERSION;
		break;
		
	case OPENPCD_CMD_GET_ENVIRONMENT:
		poh->flags &= OPENPCD_FLAG_RESPOND;
		poh->val = gen_getenv(&poh->data,poh->val);
		rctx->tot_len += poh->val;
		DEBUGP("CMD_GET_ENVIRONMENT(res_len=%u)\n", poh->val);
		break;
		
	case OPENPCD_CMD_SET_ENVIRONMENT:
		DEBUGP("CMD_SET_ENVIRONMENT (in_len=%u)\n", len);
		gen_setenv(&poh->data,len);
		break;
		
	case OPENPCD_CMD_RESET:
		DEBUGP("CMD_RESET\n");
		AT91F_RSTSoftReset(AT91C_BASE_RSTC, AT91C_RSTC_PROCRST|
				   AT91C_RSTC_PERRST|AT91C_RSTC_EXTRST);
		break;

	case OPENPCD_CMD_GET_VERSION:
		DEBUGP("GET_VERSION\n");
		poh->flags |= OPENPCD_FLAG_RESPOND;
		memcpy(ver, &opcd_version, sizeof(*ver));
		rctx->tot_len += sizeof(*ver);
		break;

	case OPENPCD_CMD_SET_LED:
		DEBUGP("SET LED(%u,%u)\n", poh->reg, poh->val);
		led_switch(poh->reg, poh->val);
		break;

	case OPENPCD_CMD_GET_SERIAL:
		DEBUGP("GET SERIAL(");
		poh->flags |= OPENPCD_FLAG_RESPOND;
#ifdef PCD
		rctx->tot_len += 4;
		if (rc632_get_serial(NULL, (u_int32_t *)poh->data) < 0) {
			DEBUGP("ERROR) ");
			return USB_ERR(USB_ERR_CMD_NOT_IMPL);
		}

		DEBUGP("%s)\n", hexdump(poh->data, 4));
#else
		/* FIXME: where to get serial in PICC case */
		return USB_ERR(USB_ERR_CMD_NOT_IMPL);
#endif
		break;

	default:
		DEBUGP("UNKNOWN\n");
		return USB_ERR(USB_ERR_CMD_UNKNOWN);
		break;
	}

	if (poh->flags & OPENPCD_FLAG_RESPOND)
		return USB_RET_RESPOND;
	return 0;
}

void usbcmd_gen_init(void)
{
	DEBUGP("Inititalizing usbcmd_gen_init\n\r");
	/* setup FLASH write support for environment storage */
	flash_init();
	
	/* retrieve default data from flash */
	memcpy(&config_stack,CONFIG_AREA_ADDR,sizeof(config_stack));
	
	usb_hdlr_register(&gen_usb_rx, OPENPCD_CMD_CLS_GENERIC);
}

