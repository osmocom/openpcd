#ifndef _RFID_H
#define _RFID_H

#include "dbgu.h"

#define rfid_hexdump hexdump

enum rfid_frametype {
	RFID_14443A_FRAME_REGULAR,
	RFID_14443B_FRAME_REGULAR,
	RFID_MIFARE_FRAME,
};

struct rfid_asic_handle {
};

struct rfid_asic {
};

#define RAH NULL

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#endif
