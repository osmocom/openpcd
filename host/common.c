/*
 * common.c
 *
 *  Created on: 27.01.2009
 *      Author: henryk
 */

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"

void Error(const char* message)
{
    fprintf(stderr,"ERROR: %s\n",message);
    exit(1);
}

void un_braindead_ify_device(int fd)
{
	/* For some stupid reason the default setting for a serial console
	 * is to use XON/XOFF. This means that some of the bytes will be
	 * dropped, making the device completely unusable for a binary protocol.
	 * Remove that setting
	 */
	struct termios options;

	tcgetattr (fd, &options);

	options.c_lflag = 0;
	options.c_iflag &= IGNPAR | IGNBRK;
	options.c_oflag &= IGNPAR | IGNBRK;

	cfsetispeed (&options, B115200);
	cfsetospeed (&options, B115200);

	if (tcsetattr (fd, TCSANOW, &options))
	{
		Error("Can't set device attributes");
	}
}

void cleanup(int fd, const char * end_command, unsigned char end_confirmation)
{
	int i;
	char d;
	if(write(fd, end_command, 1) != 1) fprintf(stderr, "Warning: Couldn't write command to end reception mode\n");
    i=0; d=0;
    while(i<4) {
    	if(read(fd, &d, sizeof(d)) == 0) break;
    	if(d == end_confirmation)
    		i++;
    	else
    		i=0;
    }
	close(fd);
}

