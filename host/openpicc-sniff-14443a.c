/***************************************************************
 *
 * OpenPICC sniffonly host application
 *
 * Copyright 2007 Milosch Meriac <meriac@bitmanufaktur.de>
 * Copyright 2007 Karsten Nohl <honk98@web.de>
 * Copyright 2007,2008 Henryk Plötz <henryk@ploetzli.ch>
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
#include <unistd.h>
#include <limits.h>
#include <termios.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


//#define DEBUG

#if defined(DEBUG)
#define DEBUGP printf
#else
#define DEBUGP(format, args...)
#endif


int print_timings = 0, max_samples = -1, print_bits = 0, read_file = 0;
const struct option options[] = {
		{"help",          no_argument, NULL, 'h'},
		{"print-timings", no_argument, &print_timings, 1},
		{"print-bits",    no_argument, &print_bits, 1},
		{"max-samples",   required_argument, NULL, 'm'},
		{"read-file",     no_argument, &read_file, 1},
		{NULL, 0, 0, 0},
};
const char *shortopts = "h?tbm:r";

#define MAX_FRAME_SIZE 1024
typedef enum { SRC_PCD, SRC_PICC } framesource;
typedef struct {
  framesource src;
  enum { TYPE_SHORT, TYPE_STANDARD } type;
  char CRC_OK, BCC_OK;
  unsigned long starttime;
  unsigned int numbytes;
  unsigned char numbits;
  unsigned short data[MAX_FRAME_SIZE]; /* Low byte is data, lsb of high byte is parity */
} frame;
frame *current_pcd_frame  = NULL;
unsigned long current_time = 0;

void end_frame(frame **target);
void Error(const char *message);

void start_frame(frame **target, framesource src) {
  if(*target != NULL) {
    fprintf(stderr, "Warning: Starting new %i frame, but last frame never ended\n", (*target)->src);
    end_frame(target);
  }
  
  *target = calloc(1, sizeof(frame));
  if(!*target) Error("calloc failed");
  (*target)->src = src;
  (*target)->starttime = current_time;
}

void append_to_frame(frame *f, unsigned char byte, char parity, unsigned char valid_bits) {
  if(f==NULL) {
    Error("Appending to NULL frame");
  }

  if(f->numbytes >= sizeof(f->data)/sizeof(f->data[0])-1) { /* -1, because the last byte may be half filled */
    Error("Frame too big");
  }
  
  if(f->numbits != 0) {
    Error("Appending to a frame with incomplete byte");
  }

  f->data[f->numbytes] = ((parity&1)<<8) | (byte&0xff);

  if(valid_bits == 8) {
    f->numbytes++;
  } else {
    f->numbits += valid_bits;
  }
}

void process_frame(frame *f);

void end_frame(frame **target) {
  if(*target == NULL) 
    return;

  if((*target)->numbytes > 0) {
    (*target)->type = TYPE_STANDARD;
  } else {
    (*target)->type = TYPE_SHORT;
  }

  if((*target)->numbytes > 0 || (*target)->numbits > 0) {
    /* Frame finished, do something with it */
    if(!print_bits) {
    	if((*target)->numbytes*8 + (*target)->numbits >= 7)
    		process_frame(*target);
    }
  }
  
  free(*target);
  *target = NULL;
}

void process_frame(frame *f) {
  static unsigned long last_time;
  long timediff = f->starttime-last_time;
  int i, j, p;
  last_time = f->starttime;

  printf("%+li: frame(%2i bytes, %i bits) from ", timediff, f->numbytes, f->numbits);
  switch(f->src) {
  case SRC_PCD:  printf("PCD   "); break;
  case SRC_PICC: printf("PICC  "); break;
  }

  if(f->CRC_OK) {
    printf("CRC OK   ");
  } else if(f->BCC_OK) {
    printf("BCC OK   ");
  } else {
    printf("         ");
  }

  for(i=0; i<f->numbytes; i++) {
    printf(" %02X %i", f->data[i]&0xff, (f->data[i]>>8) & 1);
    p = 0;
    /* j from 0 to 8 (inclusive) in order to also get the parity bit */
    for(j=0; j<9; j++) p^=( f->data[i] >> j) & 1;
    if(p!=1) printf("!"); else printf(" ");
  }

  if(f->numbits > 0) {
    printf(" %02X", f->data[f->numbytes]&0xff);
  }

  printf("\n");

}

////// PCD to PICC
#define BIT_LEN 128
#define BIT_OFFSET -4
#define BIT_LEN_3 ((BIT_LEN*3)/4 +BIT_OFFSET)
#define BIT_LEN_5 ((BIT_LEN*5)/4 +BIT_OFFSET)
#define BIT_LEN_7 ((BIT_LEN*7)/4 +BIT_OFFSET)
#define BIT_LEN_9 ((BIT_LEN*9)/4 +BIT_OFFSET)
/* The theoretical error margin for the timing measurement is about 7 (note: that is a jitter of 7 in
 * total, e.g. +/- 3.5), but we'll round that up to +/- 8. However, the specification allows pause
 * times from 2us to 3us, e.g. 1us difference, so we'll add another 13.
 */ 
#define BIT_LEN_ERROR_MAX (8+13)

#define ALMOST_EQUAL(a,b) ( abs(a-b) <= BIT_LEN_ERROR_MAX )
#define MUCH_GREATER_THAN(a,b) ( a > (b+BIT_LEN_ERROR_MAX) )
#define ALMOST_GREATER_THAN_OR_EQUAL(a,b) (a >= (b-BIT_LEN_ERROR_MAX))

int counter,byte,parity,crc;

void Error(const char* message)
{
    fprintf(stderr,"ERROR: %s\n",message);
    exit(1);
}

void Miller_End_Frame()
{
	if(current_pcd_frame != NULL) {
		if(counter > 0) {
			append_to_frame(current_pcd_frame, byte, 0, counter);
		}
		if(!crc)
			current_pcd_frame->CRC_OK = 1;
		end_frame(&current_pcd_frame);
		
		if(!crc)
			DEBUGP("%s","CRC OK");
		DEBUGP("%s","\n"); 
	}
	counter=0;
	byte=0;
	parity=0;
	crc=0x6363;
}

/*
 * Decoding Methodology: We'll only see the edges for the start of modulation pauses and not
 * all symbols generate modulation pauses at all. Two phases:
 *  + Old state and next edge delta to sequence of symbols (might be more than one symbol per edge)
 *  + Symbols to EOF/SOF marker and bits
 * 
 * These are the possible old-state/delta combinations and the symbols they yield:
 * 
 * old_state  delta (in bit_len/4)  symbol(s)
 * none       3                     ZZ
 * none       5                     ZX
 * X          3                     X
 * X          5                     YZ
 * X          7                     YX
 * X          >=9                   YY
 * Y          3                     ZZ
 * Y          5                     ZX
 * Z          3                     Z
 * Z          5                     X
 * Z          >=7                   Y
 * 
 * All other combinations are invalid and likely en- or decoding errors. (Note that old_state
 * Y is exactly the same as old_state none.)
 * 
 * The mapping from symbol sequences to SOF/EOF/bit is as follows:
 *         X: 1
 * 0, then Y: EOF
 *   other Y: 0
 *   first Z: SOF
 *   other Z: 0
 */
enum symbol {NO_SYM=0, sym_x, sym_y, sym_z};
enum bit { BIT_ERROR, BIT_SOF, BIT_0, BIT_1, BIT_EOF };
enum bit_length { out_of_range=0, len_3=1, len_5=2, len_7=3, len_9_or_greater=4 };
char *bit_length_descriptions[] = {
		[out_of_range] = "OOR",
		[len_3] = "3/4",
		[len_5] = "5/4",
		[len_7] = "7/4",
		[len_9_or_greater] = ">=9/4",
};
#define NIENTE {NO_SYM, NO_SYM}

struct decoder_table_entry { enum symbol first, second; };
const struct decoder_table_entry decoder_table[][5] = {
		         /* out_of_range len_3            len_5            len_7            len_9_or_greater*/
		[NO_SYM] = {NIENTE,      {sym_z, sym_z},  {sym_z, sym_x},  },
		[sym_x]  = {NIENTE,      {sym_x, NO_SYM}, {sym_y, sym_z},  {sym_y, sym_x},  {sym_y, sym_y}, },
		[sym_y]  = {NIENTE,      {sym_z, sym_z},  {sym_z, sym_x},  },
		[sym_z]  = {NIENTE,      {sym_z, NO_SYM}, {sym_x, NO_SYM}, {sym_y, NO_SYM}, {sym_y, NO_SYM}, },
};

void Miller_Bit(enum bit bit)
{
	switch(bit) {
	case BIT_SOF:
		if(print_bits) printf("SOF");
		start_frame(&current_pcd_frame, SRC_PCD);
		break;
	case BIT_0:
		if(print_bits) printf(" 0");
		break;
	case BIT_1:
		if(print_bits) printf(" 1");
		break;
	case BIT_EOF:
		if(print_bits) printf(" EOF\n");
		Miller_End_Frame();
		break;
	default:
		if(print_bits) printf(" ERROR\n");
		Miller_End_Frame();
		break;
	}
	
	int bit_value;
	if(bit==BIT_0) bit_value = 0;
	else if(bit==BIT_1) bit_value = 1;
	else return;
	
    if(counter<8) {
    	byte=byte | (bit_value<<counter);
    	if(bit_value)
    		parity++;
    } else DEBUGP("%s", (((~parity)&1)==bit_value) ? " ":"!" );

    if(++counter==9) {
    	counter=0;
    	append_to_frame(current_pcd_frame, byte, bit_value, 8);

    	DEBUGP(" ==0x%02X\n",byte);

    	byte=(byte ^ crc)&0xFF;
    	byte=(byte ^ byte<<4)&0xFF;
    	crc=((crc>>8)^(byte<<8)^(byte<<3)^(byte>>4))&0xFFFF;

    	//printf(" [%04X]\n",crc);

    	byte=0;        
    	parity=0;
    }
}

void Miller_Symbol(enum symbol symbol)
{
	static enum bit last_bit = BIT_ERROR;
	static int in_frame = 0;
	enum bit bit = BIT_ERROR;
	
	//DEBUGP("%c ", 'X'+(symbol-1));
	if(!in_frame) {
		if(symbol == sym_z)
			bit = BIT_SOF;
		else
			bit = BIT_ERROR;
	} else {
		switch(symbol) {
		case sym_y:
			if(last_bit == BIT_0)
				bit = BIT_EOF;
			else 
				bit = BIT_0;
			break;
		case sym_x:
			bit = BIT_1;
			break;
		case sym_z:
			bit = BIT_0;
			break;
		default:
			bit = BIT_ERROR;
			break;
		}	
	}
	
	if(bit != BIT_EOF && last_bit == BIT_0)
		Miller_Bit(last_bit);
	if(bit != BIT_0) Miller_Bit(bit);
	
	last_bit = bit;
	if(bit==BIT_SOF) {
		in_frame = 1;
		last_bit = BIT_ERROR;
	} else if(bit==BIT_EOF || bit==BIT_ERROR) {
		in_frame = 0;
	}
	
}

void Miller_Edge(unsigned int delta)
{
	static enum symbol old_state = NO_SYM;
	enum bit_length length = out_of_range;

	if( ALMOST_EQUAL(delta, BIT_LEN_3) ) {
		length = len_3;
	} else if( ALMOST_EQUAL(delta, BIT_LEN_5) ) {
		length = len_5;
	} else if( ALMOST_EQUAL(delta, BIT_LEN_7) ) {
		length = len_7;
	} else if( ALMOST_GREATER_THAN_OR_EQUAL(delta, BIT_LEN_9)) {
		length = len_9_or_greater;
	}
	
	const struct decoder_table_entry *entry;
	entry = &decoder_table[old_state][length];
	DEBUGP(" %c{%i}[%s]", 'X'-sym_x+old_state, delta, bit_length_descriptions[length]);
	if(entry->first != NO_SYM) {
		DEBUGP("%c ", 'X'-sym_x+entry->first);
		Miller_Symbol(old_state = entry->first);
	} else {
		DEBUGP("%s","! ");
	}
	
	if(entry->second != NO_SYM) {
		DEBUGP("%c ", 'X'-sym_x+entry->second);
		Miller_Symbol(old_state = entry->second);
	}
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

void cleanup(int fd)
{
	int i;
	char d;
	if(write(fd, "r", 1) != 1) fprintf(stderr, "Warning: Couldn't write command to end reception mode\n");
    i=0; d=0;
    while(i<4) {
    	if(read(fd, &d, sizeof(d)) == 0) break;
    	if(d == '-')
    		i++;
    	else 
    		i=0;
    }
	close(fd);
}

static int f=-1;
void exit_handler(int signum)
{
	if(signum == SIGINT && f!=-1) {
		fprintf(stderr, "\nReceived SIGINT, clearing up\n");
		cleanup(f);
		exit(0);
	}
}

void process_data(u_int32_t sample)
{
	if(print_timings) {
		printf(" %i", sample);
	} else {
		Miller_Edge(sample);
	}

}

void receive_openpicc(char *devicenode)
{
	int i, len, samples=0;
	char d;
	u_int32_t buffer[1024];
	
	if((f=open(devicenode, O_RDWR))==-1) {
		perror("Can't open devicenode");
		exit(1);
	}

	un_braindead_ify_device(f);

	write(f, "\n", 1);
	if(write(f, "r", 1) == 0) {
		Error("Can't write command to start reception mode");
	}
	signal(SIGINT, exit_handler);
	i=0; d=0;
	while(i<4) {
		if(read(f, &d, sizeof(d)) == 0) 
			Error("Unexpected EOF");
		if(d == '-')
			i++;
		else 
			i=0;
	}

	int do_exit=0;
	while( (len=read(f, buffer, sizeof(buffer))) > 0 && !do_exit) {
		if( len % sizeof(buffer[0]) != 0) Error("Wahh");
		len = len/sizeof(buffer[0]);
		for(i=0; i<len && !do_exit; i++) {
			if(buffer[i] == '____') {
				DEBUGP("%s","____");
				if(print_timings) printf("\n");
			} else if(buffer[i] == '////') {
				DEBUGP("%s","////");
				if(print_timings) printf("\n");
				fprintf(stderr, "Warning: Possible buffer overrun detected on the PICC\n");
			} else {
				if(buffer[i] & ~0xffff) {
					/* The value should be a 16 bit counter, so anything that uses the upper bits
					 * is likely due to a loss of synchronisation. Read single characters until we're
					 * synchronised again.
					 */
					i=0;
					fprintf(stderr,"\nResyncing (Warning: this is a communications problem and shouldn't happen).\n");
					while(i<4) {
						if(read(f, &d, sizeof(d)) == 0) 
							Error("EOF");
						else if(d == '_' || d == '/')
							i++;
						else i=0;
					}
					fprintf(stderr,"\n");
					break;
				} else {
					if(max_samples >= 0 && ++samples >= max_samples) {
						do_exit=1;
					}
					if(!do_exit) {
						process_data(buffer[i]);
					}
				}
			}
		}
	}

	cleanup(f);
	printf("\n\n");
}

void receive_file(FILE *stream)
{
	int sample;
	
	while(!feof(stream)) {
		if(fscanf(stream, "%i", &sample) != 1) {
			fgetc(stream);
		} else 
			process_data(sample);
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
					" -t, --print-timings\tPrint raw timing measurements (in carrier cycles)\n"
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
	
	Miller_End_Frame();
	
	while( (option=getopt_long(argc, argv, shortopts, options, NULL)) != 1) {
		if(option==-1) break;
		switch(option) {
		case '?': /* Fall-Through */
		case 'h':
			print_help(NULL);
			exit(0);
			break;
		case 't':
			print_timings = 1;
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
