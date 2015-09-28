
#include <librfid/rfid_layer2_iso14443a.h>

struct piccsim_state {
	enum rfid_layer2_id l2prot;
	unsigned char uid[10];
	uint8_t uid_len;
	union {
		struct {
			enum iso14443a_state state;
			enum iso14443a_level level;
			uint32_t flags;
		} iso14443a;
		struct {
		} iso14443b;
	} l2;

	union {
		uint32_t flags;
	} proto;
}

#define PICCSIM_PROT_F_AUTO_WTX		0x01
