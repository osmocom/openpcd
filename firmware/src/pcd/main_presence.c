/* AT91SAM7 "presence reader" firmware for OpenPCD
 *
 * (C) 2006 by Milosch Meriac <meriac@openpcd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <errno.h>
#include <string.h>
#include <lib_AT91SAM7.h>
#include <librfid/rfid_layer2_iso14443a.h>
#include "rc632.h"
#include <os/dbgu.h>
#include <os/led.h>
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include <pcd/rc632_highlevel.h>

#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2.h>

#include "../openpcd.h"
#include <os/main.h>

#define RAH NULL

u_int32_t delay_scan,delay_blink,last_uid,last_polled_uid;
static struct rfid_reader_handle *rh;
static struct rfid_layer2_handle *l2h;

static int usb_presence_rx(struct req_ctx *rctx)
{
    struct openpcd_hdr *poh = (struct openpcd_hdr *) rctx->data;

    switch (poh->cmd)
    {
        case OPENPCD_CMD_PRESENCE_UID_GET:
		DEBUGPCRF("get presence UID");	
		
		poh->flags |= OPENPCD_FLAG_RESPOND;
		if(last_polled_uid)
		{
            	    rctx->tot_len += 4;
		    poh->data[0]=(u_int8_t)(last_polled_uid>>24);
		    poh->data[1]=(u_int8_t)(last_polled_uid>>16);
		    poh->data[2]=(u_int8_t)(last_polled_uid>> 8);
		    poh->data[3]=(u_int8_t)(last_polled_uid    );
		    last_polled_uid=0;
		}
                break;
	default:
	        DEBUGP("UNKNOWN ");
                return USB_ERR(USB_ERR_CMD_UNKNOWN);
            	break;
    }
	
    if (poh->flags & OPENPCD_FLAG_RESPOND)
        return USB_RET_RESPOND;

    return 0;
}

void _init_func(void)
{
	DEBUGPCRF("enabling RC632");	
	rc632_init();

	DEBUGPCRF("turning on RF");
	rc632_turn_on_rf(RAH);

	DEBUGPCRF("initializing 14443A operation");
	rh = rfid_reader_open(NULL, RFID_READER_OPENPCD);
	l2h = rfid_layer2_init(rh, RFID_LAYER2_ISO14443A);
		
	DEBUGPCRF("registering USB handler");
	usb_hdlr_register(&usb_presence_rx, OPENPCD_CMD_CLS_PRESENCE);
	
	delay_scan=delay_blink=0;
	last_uid=0;
}

int _main_dbgu(char key)
{
	unsigned char value;

	switch (key) {
	case '4':
		AT91F_DBGU_Printk("Testing RC632 : ");
		if (rc632_test(RAH) == 0)
			AT91F_DBGU_Printk("SUCCESS!\n\r");
		else
			AT91F_DBGU_Printk("ERROR!\n\r");
			
		break;
	case '5':
		opcd_rc632_reg_read(RAH, RC632_REG_RX_WAIT, &value);
		DEBUGPCR("Reading RC632 Reg RxWait: 0x%02xr", value);

		break;
	case '6':
		DEBUGPCR("Writing RC632 Reg RxWait: 0x55");
		opcd_rc632_reg_write(RAH, RC632_REG_RX_WAIT, 0x55);
		break;
	case '7':
		rc632_dump();
		break;
	}

	return -EINVAL;
}

void _main_func(void)
{
	u_int32_t uid;
	int status;
	
	status = rfid_layer2_open(l2h);
        if (status >= 0  && l2h->uid_len==4)
	{
	    uid=((u_int32_t)l2h->uid[0])    |
		((u_int32_t)l2h->uid[1])<< 8|
		((u_int32_t)l2h->uid[2])<<16|
		((u_int32_t)l2h->uid[3])<<24;
			
	    delay_scan=100;
		
	    if(uid!=last_uid)
	    {
		last_uid=last_polled_uid=uid;
		delay_blink=10;
		    
		DEBUGPCR("UID:0x%08X", uid);
	    }
	}
	else
	    if(delay_scan)
		delay_scan--;
	    else
		last_uid=0;
	    
        led_switch(1,(delay_blink==0)?1:0);
	if(delay_blink)
	    delay_blink--;
	
	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();
	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	usb_in_process();
	rc632_unthrottle();
}
