/***************************************************************
 *
 * OpenPICC sniff-both host application
 *
 * Copyright 2007 Milosch Meriac <meriac@bitmanufaktur.de>
 * Copyright 2007 Karsten Nohl <honk98@web.de>
 * Copyright 2007-2009 Henryk Plötz <henryk@ploetzli.ch>
 *
 ***************************************************************

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <termios.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"

#define DEBUG

#if defined(DEBUG)
#define DEBUGP printf
#else
#define DEBUGP(format, args...)
#endif

#define OVERFLOW_MAGIC 0xAA64AAAA

static int print_raw = 0, max_samples = -1, print_bits = 0, read_file = 0;
static const struct option options[] = {
		{"help",          no_argument, NULL, 'h'},
		{"print-raw",     no_argument, &print_raw, 1},
		{"print-bits",    no_argument, &print_bits, 1},
		{"max-samples",   required_argument, NULL, 'm'},
		{"read-file",     no_argument, &read_file, 1},
		{NULL, 0, 0, 0},
};
static const char *shortopts = "h?Rbm:r";

#define MAX_FRAME_SIZE 1024

static int f=-1;
static volatile int do_abort = 0;
static volatile int stop_now = 0;
void exit_handler(int signum)
{
	if((signum == SIGINT||signum == SIGPIPE) && f!=-1) {
		if(!do_abort) {
			fprintf(stderr, "\nReceived signal, exiting\n");
			do_abort = 1;
		} else {
			fprintf(stderr, "\nReceived signal again, forcing the issue\n");
			exit(0);
		}
	}
	if(signum == SIGPIPE) stop_now = 1;
}

/* Masked Equal */
#define MEQ(a,b) (((a)&(b))==(b))

static int last_pcd = -1, last_picc=-1;
static int time_diff = 0;
#define SAMPLE_CLOCK (47923200/16)

static u_int32_t filter_history = 0;
void process_sample(u_int8_t sample)
{
	filter_history = ((filter_history<<1) | sample);
	int pcd = MEQ(filter_history,0x7);
#if 0
	int picc = !!(filter_history&(0x7f<<2))
		&& !MEQ(filter_history,0x7<<0)
		&& !MEQ(filter_history,0x7<<1)
		&& !MEQ(filter_history,0x7<<2)
		&& !MEQ(filter_history,0x7<<3)
		&& !MEQ(filter_history,0x7<<4)
		&& !MEQ(filter_history,0x7<<5)
		&& !MEQ(filter_history,0x7<<6)
		&& !MEQ(filter_history,0x7<<7)
		&& !MEQ(filter_history,0x7<<8);
#else
	int picc;
#define PICC_POINTER (1<<2)
#define PICC_MASK (0x1f<<2)
	if(last_picc == -1) picc = !!(filter_history & PICC_POINTER);
	else if(!pcd && !MEQ(filter_history,0x7<<5)
			&& !MEQ(filter_history,0x7<<6)
			&& !MEQ(filter_history,0x7<<7)
			&& !MEQ(filter_history,0x7<<8)) {
		if(last_picc) {
			if(!(filter_history&PICC_MASK)) picc = 0;
			else picc = 1;
		} else {
			if(filter_history & PICC_POINTER) picc = 1;
			else picc = 0;
		}
	} else picc = 0;
#endif

	//if(!stop_now) fprintf(stderr, "%i %i %i\n", sample, pcd, picc);
	if(pcd != last_pcd || picc != last_picc) {
		if(!stop_now) printf("%llu,%i,%i\n", (unsigned long long)(((double)time_diff/(double)SAMPLE_CLOCK)*1E9), pcd, picc);
		time_diff = 0;
	}
	last_pcd = pcd; last_picc = picc;
	time_diff += 1;
}

void process_word(u_int32_t sample)
{
	static int zero_samples = 0;
	int i;
	if(sample == 0) zero_samples++;
	else zero_samples = 0;

	if(zero_samples < 10) {
	    for(i=0; i<32; i++) {
		    process_sample(sample & 1);
		    sample >>= 1;
	    }
	}
}

void process_data(u_int32_t sample)
{
	static int overflow_filter = 0;
	int i;
	if(print_raw) {
		if(!stop_now) printf("%i\n", sample);
	} else {
		if(sample == OVERFLOW_MAGIC) {
			overflow_filter++;
			if(overflow_filter == 4) {
				fprintf(stderr, "Warning: Overflow detected, some samples have been lost\n");
				overflow_filter = 0;
			}
		} else {
			if(overflow_filter > 0) {
				for(i=0; i<overflow_filter; i++)
					process_word(OVERFLOW_MAGIC);
				overflow_filter = 0;
			}

			process_word(sample);
		}
	}
}

void receive_openpicc(char *devicenode)
{
	int i, j, len, samples=0;
	char d;
	u_int32_t buffer[1024];

	if((f=open(devicenode, O_RDWR))==-1) {
		perror("Can't open devicenode");
		exit(1);
	}

	un_braindead_ify_device(f);

	write(f, "\n", 1);
	if(write(f, "s", 1) == 0) {
		Error("Can't write command to start reception mode");
	}
	signal(SIGINT, exit_handler);
	signal(SIGPIPE, exit_handler);
	i=0; j=0; d=0;
	while(i<4) {
		if(read(f, &d, sizeof(d)) == 0)
			Error("Unexpected EOF");
		if(d == '{') {
			i++; j=0;
		} else if(d == '}') {
			j++; i=0;
		} else
			i=j=0;
		if(j==4) {
			fprintf(stderr, "Warning: Device is in wrong state, recovering\n");
			if(write(f, "s", 1) == 0) {
				Error("Can't write command to start reception mode");
			}
			i=j=0;
		}
		if(do_abort) {
			cleanup(f, "s", '}');
			return;
		}
	}

	int do_exit=0;
	int offset = 0, next_offset;
	while( (len=read(f, (char*)buffer+offset, sizeof(buffer)-offset)) > 0 && !do_exit) {
		len = len + offset;
		next_offset = len % sizeof(buffer[0]);
		int count = (len - next_offset)/sizeof(buffer[0]);
		for(i=0; i<count && !do_exit; i++) {
			if(max_samples >= 0 && ++samples >= max_samples) {
				do_exit=1;
			}
			if(do_abort) {
				do_exit = 1;
			}

			if(!do_exit) {
				process_data(buffer[i]);
			}
		}
		if(next_offset > 0) {
			memcpy(buffer, (char*)buffer+len-next_offset, next_offset);
			offset = next_offset;
		} else offset =0;
		fflush(stdout);
		fflush(stderr);
	}

	cleanup(f, "s", '}');
	if(!stop_now) printf("\n\n");
}

void receive_file(FILE *stream)
{
	int sample;

	while(!feof(stream)) {
		if(fscanf(stream, "%i", &sample) != 1) {
			fgetc(stream);
		} else
			process_data(sample);
		if(do_abort)
			break;
	}
}

void print_help(const char* message)
{
	if(message != NULL) {
		fprintf(stderr, "Error: %s\n", message);
	}

	fprintf(stderr, "Usage: openpicc-sniff-14443a [OPTIONS] devicenode\n"
					"   or  openpicc-sniff-14443a -r|--read-file [filename]\n"
			        "   or  openpicc-sniff-14443a --help|-h|-?  to print this help\n"
					"\n"
					"Options:\n"
					" -R, --print-raw    \tPrint raw samples\n"
					"                    \tinstead of decoded frames\n"
					" -b, --print-bits   \tPrint raw bit stream (in transmission order) and\n"
					"                    \tnot the decoded frames\n"
					" -m, --max-samples n\tStop sampling after acquiring n samples\n"
					" -r, --read-file    \tDo not connect to an OpenPICC, instead read from\n"
					"                    \tfile or stdin. The input are numeric samples\n"
					"                    \tseparated by non-numeric characters, like the\n"
					"                    \toutput of --print-timings.\n"
			);
}

int main(int argc, char *argv[])
{
	int option;

	while( (option=getopt_long(argc, argv, shortopts, options, NULL)) != 1) {
		if(option==-1) break;
		switch(option) {
		case '?': /* Fall-Through */
		case 'h':
			print_help(NULL);
			exit(0);
			break;
		case 'R':
			print_raw = 1;
			break;
		case 'b':
			print_bits = 1;
			break;
		case 'm':
			max_samples = atoi(optarg);
			break;
		case 'r':
			read_file = 1;
			break;
		}
	}

	if(optind == argc && !read_file) {
		print_help("No devicenode specified");
		exit(1);
	} else if(optind+1 < argc) {
		print_help("Extraneous arguments");
		exit(1);
	}

	if(!print_raw) printf("Time,PCD->PICC,PICC->PCD \n");
	if(read_file) {
		if(optind+1 == argc) {
			FILE *stream = fopen(argv[optind], "r");
			if(stream == NULL) {
				perror("Couldn't open file for reading");
			} else {
				receive_file(stream);
				fclose(stream);
			}
		} else {
			receive_file(stdin);
		}
	} else {
		receive_openpicc(argv[optind]);
	}

	return 0;
}
