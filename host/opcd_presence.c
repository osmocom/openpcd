/* opcd_test - Low-Level test program for OpenPCD
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define __user

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <usb.h>

#include <openpcd.h>
#include "opcd_usb.h"

#include <curl/curl.h>

int main(int argc, char **argv)
{
	struct opcd_handle *od;
	int retlen;
	unsigned int uid;
	unsigned char *data;
	static unsigned char buf[8192];
	CURL *curl;
	CURLcode res;
	
	curl = curl_easy_init();
	if(!curl)
	{
	    printf("Can't open CURL library\n");
	    exit(1);
	}
	
	curl_easy_setopt(curl, CURLOPT_URL, "http://medusa.benutzerserver.de/openpcd.announce.php");
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	
	od = NULL;

	while (1)
	{
	    if(od)
	    {
		retlen = opcd_send_command(od, OPENPCD_CMD_PRESENCE_UID_GET, 0, 0, 0, NULL);    
		if(retlen<=0)
		{
		    opcd_fini(od);
		    od=NULL;
		}
		else
		{
		    retlen = opcd_recv_reply(od, (char*)buf, sizeof(struct openpcd_hdr)+4);
		    if (retlen == (sizeof(struct openpcd_hdr)+4) )
		    {
			data = buf + sizeof(struct openpcd_hdr);
		
			uid=	((unsigned int)data[0])<<24 |
				((unsigned int)data[1])<<16 |
				((unsigned int)data[2])<< 8 |
				((unsigned int)data[3]);

			sprintf((char*)buf,"uid=%08X",uid);
			printf("%s\n",buf);
			
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf);
			res = curl_easy_perform(curl);
			if(res)
			    printf("CURL: error(%i)\n",res);
		    }
		}
	    }
	    else
	    {
		printf("STATUS: reinitializing\n");
		od = opcd_init(0);
	    }
	    // sleep for 250ms
	    usleep(1000*250);
	}
	
	  /* always cleanup */
	curl_easy_cleanup(curl);
	
	return(0);
}
