/* (C) 2007 Milosch Meriac <meriac@bitmanufaktur.de>, Karsten Nohl <honk98@web.de>, Henryk Plötz <henryk@ploetzli.ch> */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

// Util functions
#define FROM_PCD "\n>> "
#define FROM_PICC "\n<< "
#define FROM_DITTO "\n   "

char *get_dir_prefix(char* dir, int with_newline) {
  static char* last_dir;
  static int makenew;
  if(dir != last_dir) {
    last_dir = dir;
    makenew = with_newline;
    return dir;
  } else if(makenew) {
    makenew = with_newline;
    return FROM_DITTO;
  } else {
    if(with_newline) makenew = 1;
    return "";
  }
}

//#define DEBUG
#ifdef DEBUG
#define directed_print(dir, s, args...) printf("%s" s, get_dir_prefix(dir, 0), ##args)
#define directed_println(dir, s, args...) printf("%s" s , get_dir_prefix(dir, 1), ##args)
#else
#define directed_print(...)
#define directed_println(...)
#endif

#define MAX_FRAME_SIZE 1024
typedef enum { SRC_PCD, SRC_PICC } framesource;
typedef struct {
  framesource src;
  enum { TYPE_SHORT, TYPE_STANDARD } type;
  char CRC_OK, BCC_OK;
  unsigned long starttime;
  unsigned long starttime_poweron;
  unsigned long starttime_rf_poweron;
  unsigned int numbytes;
  unsigned char numbits;
  unsigned short data[MAX_FRAME_SIZE]; /* Low byte is data, lsb of high byte is parity */
} frame;
frame *current_picc_frame = NULL;
frame *current_pcd_frame  = NULL;
unsigned long current_time = 0;
unsigned long poweron_time = 0;
unsigned long rf_poweron_time = 0;

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
  (*target)->starttime_poweron = poweron_time;
  (*target)->starttime_rf_poweron = rf_poweron_time;
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
    process_frame(*target);
  }

  free(*target);
  *target = NULL;
}

void process_frame(frame *f) {
  static unsigned long last_time;
  long timediff = f->starttime-last_time;
  int i, j, p, __attribute__((unused)) toprint;
  last_time = f->starttime;

  printf("%+15li", timediff);
  if(f->starttime_poweron != 0)
	  printf(" (%+15li since PCD power on)", f->starttime_poweron);
  if(f->starttime_rf_poweron != 0)
	  printf(" (%+15li since RF power on)", f->starttime_rf_poweron);
  printf(": Got frame of %2i bytes and %i bits from ", f->numbytes, f->numbits);
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

  int parity_error = 0;
  for(i=0; i<f->numbytes; i++) {
    printf(" %02X %i", f->data[i]&0xff, (f->data[i]>>8) & 1);
    p = 0;
    /* j from 0 to 8 (inclusive) in order to also get the parity bit */
    for(j=0; j<9; j++) p^=( f->data[i] >> j) & 1;
    if(p!=1) {
    	printf("!");
    	parity_error = 1;
    } else printf(" ");
  }

  if(f->numbits > 0) {
    printf(" %02X", f->data[f->numbytes]&0xff);
  }

#define INTERPRETATION " : "
  static enum {
	  LAST_STATE_NONE,
	  LAST_STATE_SELECTED,
	  LAST_STATE_AUTH1,
	  LAST_STATE_PRNG,
	  LAST_STATE_AUTH2,
	  LAST_STATE_ENC
  } last_state = LAST_STATE_NONE;

  switch(last_state) {
  case LAST_STATE_NONE:
  case LAST_STATE_SELECTED:
	  if(f->src == SRC_PCD && f->numbytes == 9 && f->numbits == 0 && f->CRC_OK && (f->data[0] & 0xff) == 0x93) {
		  printf(INTERPRETATION "SELECT %02X%02X%02X%02X", f->data[2] & 0xff, f->data[3] & 0xff, f->data[4] & 0xff, f->data[5] & 0xff);
		  last_state = LAST_STATE_SELECTED;
	  } else if( (f->src == SRC_PCD && !( (f->numbytes == 0 && f->numbits == 7) || (f->numbytes >= 2) ))  ||
			  (f->src == SRC_PICC && (f->numbytes < 4 || f->numbits != 0) ) ||
			  (f->numbytes > 18) ) {
		  printf(INTERPRETATION "LIKELY FALSE");
	  } else if(f->src == SRC_PCD && f->numbytes == 4 && f->numbits == 0 && f->CRC_OK && (f->data[0]&0xfe) == 0x60) {
		  printf(INTERPRETATION "AUTH1%s %02X ", ((f->data[0] & 1) == 0) ? "A" : "B", f->data[1] & 0xff);
		  last_state = LAST_STATE_AUTH1;
	  } else if( !(f->src == SRC_PICC && f->numbytes == 3 && f->numbits == 0) )
		  last_state = LAST_STATE_NONE;
	  break;
  case LAST_STATE_AUTH1:
	  if(f->src == SRC_PICC && f->numbytes == 4 && f->numbits == 0 && parity_error == 0) {
		  printf(INTERPRETATION "PRNG1 %02X%02X%02X%02X", f->data[0] & 0xff, f->data[1] & 0xff, f->data[2] & 0xff, f->data[3] & 0xff);
		  last_state = LAST_STATE_PRNG;
	  } else last_state = LAST_STATE_NONE;
	  break;
  case LAST_STATE_PRNG:
	  if(f->src == SRC_PCD && f->numbytes == 8 && f->numbits == 0) {
		  printf(INTERPRETATION "AUTH2 %02X%02X%02X%02X %02X%02X%02X%02X", f->data[0] & 0xff, f->data[1] & 0xff, f->data[2] & 0xff, f->data[3] & 0xff,
				  f->data[4] & 0xff, f->data[5] & 0xff, f->data[6] & 0xff, f->data[7] & 0xff);
		  last_state = LAST_STATE_AUTH2;
	  } else last_state = LAST_STATE_NONE;
	  break;
  case LAST_STATE_AUTH2:
	  if(f->src == SRC_PICC && f->numbytes == 4 && f->numbits == 0) {
		  printf(INTERPRETATION "PRNG2 %02X%02X%02X%02X", f->data[0] & 0xff, f->data[1] & 0xff, f->data[2] & 0xff, f->data[3] & 0xff);
		  last_state = LAST_STATE_ENC;
	  } else last_state = LAST_STATE_NONE;
	  break;
  case LAST_STATE_ENC:
	  if(f->numbits == 0) {
		  printf(INTERPRETATION "ENC");
	  } else last_state = LAST_STATE_NONE;
  default:
	  break;
  }

  printf("\n");

}

////// PCD to PICC
#define BIT_LEN 9440
#define BIT_LEN_3 ((BIT_LEN*3)/4)
#define BIT_LEN_5 ((BIT_LEN*5)/4)
#define BIT_LEN_7 ((BIT_LEN*7)/4)
#define BIT_LEN_MAX ((BIT_LEN*9)/4)
#define SPLIT_Z 0

typedef enum {MILLER_X=0,MILLER_Y,MILLER_Z} TMillerState;

int counter,byte,parity,crc;
TMillerState MillerStatePrev;

void Error(const char* message)
{
    printf("ERROR: %s\n",message);
    exit(1);
}

void DiffMiller_Bit(int bit,int delta)
{
	int __attribute__((unused)) delta_out = (delta * 4) / BIT_LEN;
    directed_print(FROM_PCD, "[%02i]%i ",delta_out,bit);

    if(counter<8)
    {
      byte=byte | (bit<<counter);
	if(bit)
	    parity++;
    }
    else
      directed_print(FROM_PCD, "%s", (((~parity)&1)==bit) ? " ":"!" );

    if(++counter==9)
    {
        counter=0;
	append_to_frame(current_pcd_frame, byte, bit, 8);

        directed_println(FROM_PCD, " ==0x%02X",byte);

        byte=(byte ^ crc)&0xFF;
        byte=(byte ^ byte<<4)&0xFF;
	crc=((crc>>8)^(byte<<8)^(byte<<3)^(byte>>4))&0xFFFF;

//        printf(" [%04X]\n",crc);

        byte=0;
        parity=0;
    }
}

// delta = time [in 10^-5 seconds] BETWEEN rising delta
void DiffMiller_Edge(unsigned int delta, int _old_state, int _new_state)
{
    int bit=0;

    if(delta>BIT_LEN_MAX)
    {
      if(current_pcd_frame != NULL) {
	if(counter > 0) {
	  append_to_frame(current_pcd_frame, byte, 0, counter);
	}
	if(!crc)
	  current_pcd_frame->CRC_OK = 1;
	end_frame(&current_pcd_frame);
      }
		if(!crc)
		    directed_print(FROM_PCD, "CRC OK");
		directed_println(FROM_PCD, "");
		counter=0;
		byte=0;
		parity=0;
		crc=0x6363;
		MillerStatePrev=MILLER_Y;
    }
    else
    {
		directed_print(FROM_PCD, "%c", ('X'+(char)MillerStatePrev));

		switch(MillerStatePrev)
		{
		    case MILLER_X:
				if(delta<BIT_LEN_5)
				{
				    bit=1;
				}
				else
				{
				    if(delta>BIT_LEN_7)
				    {
				    	if(SPLIT_Z)
				    	{
							delta -= BIT_LEN;
							DiffMiller_Bit(0,BIT_LEN);
				    	}else{
							DiffMiller_Bit(0,0);
				    	}
				    	//putchar(' ');
						bit=1;
						directed_print(FROM_PCD, "x");
					    }
					    else
					    {
						MillerStatePrev=MILLER_Z;
						bit=0;
					    }
				}
			break;
		    case MILLER_Y:
				directed_println(FROM_PCD, "");
				if(delta<BIT_LEN_5)
				{
				    MillerStatePrev=MILLER_Z;
				    bit=-1;
				}
				else
				{
					directed_print(FROM_PCD, "x");
				    MillerStatePrev=MILLER_X;
				    bit=1;
				}
			break;
		    case MILLER_Z:
				if(delta<BIT_LEN_5)
				{
				    bit=0;
				}
				else
				    if(delta>BIT_LEN_7)
				    {
						directed_print(FROM_PCD, "[encoding error]");
						bit=-1;
				    }
				    else
				    {
				    	if(SPLIT_Z)
				    	{
							delta -= BIT_LEN;
							DiffMiller_Bit(0,BIT_LEN);
				    	}else{
							DiffMiller_Bit(0,0);
				    	}
						//putchar(' ');
						MillerStatePrev=MILLER_X;
						bit=1;
						directed_print(FROM_PCD, "z");
				    }
				break;
		}

		if(bit>=0)
		  {
		    if(current_pcd_frame == NULL)
		      start_frame(&current_pcd_frame, SRC_PCD);
		    DiffMiller_Bit(bit,delta);
		  }

    }
}

////// PICC to PCD
// In nanoseconds
#define PICC_BASE_TIME_UNIT 9440
#define PICC_HALF_TIME_UNIT (PICC_BASE_TIME_UNIT/2)
#define PICC_DISPLAY_TIME_UNIT (PICC_BASE_TIME_UNIT/4)
#define PICC_FUZZYNESS (PICC_BASE_TIME_UNIT/8)
#define PICC_APPROX_EQUAL(x,y)  ((x > y-PICC_FUZZYNESS) && (x < y+PICC_FUZZYNESS))

void picc_do_bit(int bit, int reset) {
  static int byte, numbits, parity;
  static int BCC, CRC, numbytes;;
  if(reset > 0) { /* Reset byte */
    if(reset==1 && numbits > 0) {
      append_to_frame(current_picc_frame, byte, 0, numbits);
    }

    byte = numbits = parity = 0;
    if(reset > 1) { /* Reset frame */
      if(numbytes > 0) {
	if(CRC==0) {
	  current_picc_frame->CRC_OK = 1;
	  directed_print(FROM_PICC, "CRC OK");
	} else if(BCC==0) { /* Last byte in UID response to Select command is BCC (checksum as exclusive OR of all UID bytes) */
	  current_picc_frame->BCC_OK = 1;
	  directed_print(FROM_PICC, "BCC OK");
	}
      }
      BCC = numbytes = 0;
      CRC = 0x6363;
    }
  } else {
    numbits++;
    parity ^= bit;
    if(numbits <= 8) {
      byte = byte | (bit<<(numbits-1));
    }

    directed_print(FROM_PICC, "%i ", bit);

    if(numbits == 9) {
      append_to_frame(current_picc_frame, byte, bit, 8);
      directed_println(FROM_PICC, "%s ==0x%02X", parity==1 ? " " : "!", byte);

      numbytes++;
      /* Update BCC */
      BCC ^= byte;

      /* Update CRC */
      byte=(byte ^ CRC)&0xFF;
      byte=(byte ^ byte<<4)&0xFF;
      CRC=((CRC>>8)^(byte<<8)^(byte<<3)^(byte>>4))&0xFFFF;

      byte = numbits = parity = 0;
    }
  }
}

#undef EOF
typedef enum {
  NONE, SOF, ZERO, ONE, EOF,
} PICC_symbol;
typedef enum {
  N1, N2, H, F // Not in frame (levels 1 and 2); Half period; Full period
} PICC_state;

void handle_picc(unsigned int time_since_last_change, int old_state, int new_state) {
  //fprintf(stderr, "From %i to %i after %u ns on PICC\n", old_state, new_state, time_since_last_change);
  static PICC_state state;
  PICC_symbol symbol_emitted = NONE;
  PICC_state prev_state = state;

  switch(state) {
  case N1:
    if (time_since_last_change > PICC_BASE_TIME_UNIT + PICC_FUZZYNESS) {
      state = N2;
    } else {
      fprintf(stderr, "Funky: PICC: Not in frame(N1), but delta is %u (fuzzy smaller %u)\n", time_since_last_change, PICC_BASE_TIME_UNIT);
    }
    break;
  case N2:
    if ( PICC_APPROX_EQUAL(time_since_last_change, PICC_HALF_TIME_UNIT) ) {
      symbol_emitted = SOF;
      state = H;
    } else {
      if (time_since_last_change > PICC_BASE_TIME_UNIT + PICC_FUZZYNESS) {
        state = N2;
      } else {
        fprintf(stderr, "Funky: PICC: Not in frame(N2), but delta is %u (not APPROX_EQUAL %u)\n", time_since_last_change, PICC_HALF_TIME_UNIT);
        state = N1;
      }
    }
    break;
  case H:
    if ( PICC_APPROX_EQUAL(time_since_last_change, PICC_HALF_TIME_UNIT) ) {
      state = F;
    } else if ( PICC_APPROX_EQUAL(time_since_last_change, PICC_BASE_TIME_UNIT) ) {
      switch(new_state) {
      case 0:
	symbol_emitted = ONE;
	break;
      case 1:
	symbol_emitted = ZERO;
	break;
      }
      state = H;
    } else if ( time_since_last_change > PICC_BASE_TIME_UNIT + PICC_FUZZYNESS) {
      symbol_emitted = EOF;
      state = N2;
    } else {
      fprintf(stderr, "Funky: PICC: State U, but delta is %u\n", time_since_last_change);
    }
    break;
  case F:
    if ( PICC_APPROX_EQUAL(time_since_last_change, PICC_HALF_TIME_UNIT) ) {
      switch(new_state) {
      case 0:
	symbol_emitted = ONE;
	break;
      case 1:
	symbol_emitted = ZERO;
	break;
      }
      state = H;
    } else if ( PICC_APPROX_EQUAL(time_since_last_change, PICC_BASE_TIME_UNIT) ) {
      symbol_emitted = EOF;
      state = N1;
    } else if ( time_since_last_change > PICC_BASE_TIME_UNIT + PICC_FUZZYNESS) {
      symbol_emitted = EOF;
      state = N2;
    } else {
      fprintf(stderr, "Funky: PICC: State O, but delta is %u\n", time_since_last_change);
    }
    break;
  }

  //fprintf(stderr, "%i %i\n", state, symbol_emitted);

  if(symbol_emitted != NONE) {
    switch(prev_state) {
    case N1: directed_print(FROM_PICC, "n"); break;
    case N2: directed_print(FROM_PICC, "N"); break;
    case H: directed_print(FROM_PICC, "H"); break;
    case F: directed_print(FROM_PICC, "F"); break;
    }
    directed_print(FROM_PICC, "[%02i]", (time_since_last_change/PICC_DISPLAY_TIME_UNIT > 99 ? 99 : time_since_last_change/PICC_DISPLAY_TIME_UNIT));
  }

  switch(symbol_emitted) {
  case NONE: break;
  case SOF:
    start_frame(&current_picc_frame, SRC_PICC);
    directed_println(FROM_PICC, "SOF");
    break;
  case ZERO:
    picc_do_bit(0, 0);
    break;
  case ONE:
    picc_do_bit(1, 0);
    break;
  case EOF:
    picc_do_bit(0, 1);
    directed_println(FROM_PICC, "EOF");
    picc_do_bit(0, 2);
    end_frame(&current_picc_frame);
    break;
  }
}
#define EOF (-1)

void time_channel(unsigned int time_since_last_change, int old_state, int new_state) {
	/* Dummy function used as magic value below */
}

void pcd_power_channel(unsigned int time_since_last_change, int old_state, int new_state) {
	if(new_state == 0) poweron_time = 0;
	else poweron_time += time_since_last_change;
}

void rf_power_channel(unsigned int time_since_last_change, int old_state, int new_state) {
	static int state = 0;
	if(state == 0) rf_poweron_time = 0;
	else rf_poweron_time += time_since_last_change;
	state = new_state;
}

static const char *time_channel_names[] = { "Time", NULL };
static const char *picc_channel_names[] = {	"PICC->PCD", "MFOUT", NULL };
static const char *pcd_channel_names[] =  { "PCD->PICC", "SSC_DATA", NULL };
static const char *pcd_power_channel_names[] =  { "PCD_POWER", NULL };
static const char *rf_power_channel_names[] =  { "RF_POWER", NULL };

static struct channels {
  int column_index;
  unsigned int last_state;
  unsigned int state_duration;
  unsigned int time_since_high;
  enum { ALL, CHANGE, HIGH } handler_type;
  void (*handler)(unsigned int, int, int); // delta, old state, new state
  unsigned int timeout;
  int timed_out;
  const char **names;
} channels[] =  {
  {-1, -1, 0, 0, ALL, time_channel, 0, 0, time_channel_names}, /* Must be first (and index TIME_CHANNEL_INDEX) */
  {-1, -1, 0, 0, ALL, pcd_power_channel, 0, 0, pcd_power_channel_names},
  {-1, -1, 0, 0, ALL, rf_power_channel, 0, 0, rf_power_channel_names},
  {-1, -1, 0, 0, CHANGE, handle_picc, PICC_BASE_TIME_UNIT + PICC_FUZZYNESS + 1, 0, picc_channel_names},
  {-1, -1, 0, 0, HIGH, DiffMiller_Edge, BIT_LEN_MAX, 0, pcd_channel_names},
};
const static int DEFINEDCHANNELS = sizeof(channels)/sizeof(channels[0]);
#define TIME_CHANNEL_INDEX 0
static int numchannels = 0;

#define MAXCOLUMNS 10

#define CSV_DELIMITER ","

int main(int argc, char *argv[])
{
    int i,d;
    unsigned int delta,data[MAXCOLUMNS];
    char buffer[1024];
    FILE *f;

    if(argc!=2)
	Error("command line is 'decode-14443a datafile.csv'\n");

    if((f=fopen(argv[1],"rb"))==NULL)
	Error("can't open data file");

    if(fgets(buffer,sizeof(buffer),f)==NULL) {
    	Error("empty file");
    } else {
    	char *old_pos = buffer, *pos;
    	int did_rest = 0;
    	numchannels = 0;

    	while( (pos=index(old_pos, CSV_DELIMITER[0])) != NULL || !did_rest) {
    		if(numchannels >= MAXCOLUMNS ) Error("Too many column headers found");

    		if(pos == NULL) {
    			pos = buffer + strlen(buffer);
    			while(*pos == '\n' || *pos == '\0') *pos-- = 0;
    			did_rest = 1;
    		}

    		if(pos-old_pos == 0) {
    			fprintf(stderr, "Warning: Empty column header found, ignoring column %i\n", numchannels);
    		} else {
    			char *channel_name = malloc(pos-old_pos +1);
    			if(channel_name == NULL) Error("malloc failed");
    			strncpy(channel_name, old_pos, pos-old_pos);
    			channel_name[pos-old_pos] = 0;

    			int i, j, identified_channel = 0;
    			for(i=0,j=0; i<DEFINEDCHANNELS; i++,j=0) {
    				if(channels[i].names == NULL) continue;
    				do {
    					if(strcmp(channels[i].names[j], channel_name) == 0) {
    						if(channels[i].column_index == -1)
    							channels[i].column_index = numchannels;
    						else
    							fprintf(stderr, "Warning: Channel definition matches more than two columns (first matched column %i, current column %i), using first match\n", channels[i].column_index, numchannels);
    						identified_channel = 1;
    					}
    				} while(channels[i].names[++j] != NULL);
    			}

    			if(!identified_channel) {
    				fprintf(stderr, "Warning: Could not assign channel %i named '%s' to a channel definition, ignoring channel\n", numchannels, channel_name);
    			}

    			free(channel_name);
    		}
			numchannels++;
			old_pos = pos+1;
    	}
    }

    if(channels[TIME_CHANNEL_INDEX].handler != time_channel || channels[TIME_CHANNEL_INDEX].column_index == -1)
    	Error("No time channel found");

    int file_eof = 0;
    while (!file_eof) {
    	d=0;
    	memset(&data, 0, sizeof(data));
    	while(d<numchannels) {
    		int retval = fscanf(f, (d==numchannels-1 ? "%u" : "%u" CSV_DELIMITER), &data[d] );
    		if(retval == EOF) {
    			file_eof=1;
    			break;
    		}
    		d += retval;
    	}
    	if(file_eof) break;

    	delta = data[channels[TIME_CHANNEL_INDEX].column_index];
    	current_time += delta;

    	for(i=0; i<DEFINEDCHANNELS; i++) {
    		if(channels[i].column_index == -1) continue;

    		channels[i].state_duration += delta;
    		if(channels[i].handler != NULL && channels[i].handler_type == ALL) {
    			channels[i].handler(delta, -1, data[channels[i].column_index]);
    		} else if(data[channels[i].column_index] != channels[i].last_state) {
    			channels[i].timed_out = 0;
    			if(channels[i].handler == NULL)
    				continue;

				switch(channels[i].handler_type) {
					case ALL:
						channels[i].handler(channels[i].state_duration, channels[i].last_state, data[channels[i].column_index]);
						break;
					case CHANGE:
						channels[i].handler(channels[i].state_duration, channels[i].last_state, data[channels[i].column_index]);
						break;
					case HIGH:
						channels[i].time_since_high += channels[i].state_duration;
						if(data[channels[i].column_index] > 0) {
							channels[i].handler(channels[i].time_since_high, channels[i].last_state, data[channels[i].column_index]);
							channels[i].time_since_high=0;
						}
				}

	    		channels[i].last_state = data[channels[i].column_index];
	    		channels[i].state_duration = 0;
    		} else if(channels[i].handler != NULL && !channels[i].timed_out) {
    			switch(channels[i].handler_type) {
    				case ALL: break;
    				case CHANGE:
    					if(channels[i].state_duration > channels[i].timeout) {
    						channels[i].handler(channels[i].state_duration, channels[i].last_state, data[channels[i].column_index]);
	    					channels[i].timed_out = 1;
    					}
    					break;
    				case HIGH:
    					if(channels[i].time_since_high + channels[i].state_duration > channels[i].timeout) {
    						channels[i].handler(channels[i].time_since_high+channels[i].state_duration, channels[i].last_state, data[channels[i].column_index]);
	    					channels[i].timed_out = 1;
    					}
    					break;
    			}
    		}
    	}
    }
    fclose(f);

    printf("\n\n");

    return 0;
}
