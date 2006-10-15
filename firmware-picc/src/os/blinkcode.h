#ifndef _BLINKCODE_H
#define _BLINKCODE_H

enum blinkcode_num {
	BLINKCODE_NONE,
	BLINKCODE_IDLE,
	BLINKCODE_DFU_MODE,
	BLINKCODE_DFU_PROBLEM,
	BLINKCODE_
};

extern void blinkcode_set(int led, enum blinkcode_num);
extern void blinkcode_init(void);
#endif
