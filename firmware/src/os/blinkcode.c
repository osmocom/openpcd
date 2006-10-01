/* SAM7DFU blink code support 
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
 *
 */

#include <os/pit.h>
#include <os/dbgu.h>
#include <os/led.h>
#include <os/blinkcode.h>

#define NUM_LEDS 2

enum blinkcode_state {
	BLINKCODE_STATE_NONE,
	BLINKCODE_STATE_SILENT,		/* period of silence at start */
	BLINKCODE_STATE_INIT,		/* initial long light */
	BLINKCODE_STATE_BLINK_OFF,	/* blinking out, currently off */
	BLINKCODE_STATE_BLINK_ON,	/* blinking out, currently on */
	BLINKCODE_STATE_DONE,
};

#define TIME_SILENT	(1000)
#define TIME_INIT	(1000)
#define TIME_BLINK	(100)

struct blinker {
	struct timer_list timer;
	enum blinkcode_state state;
	int8_t num;
	u_int8_t led;
};

static struct blinker blink_state[NUM_LEDS];

static void blinkcode_cb(void *data)
{
	/* we got called back by the timer */
	struct blinker *bl = data;

	switch (bl->state) {
	case BLINKCODE_STATE_NONE:
		led_switch(bl->led, 0);
		bl->state = BLINKCODE_STATE_SILENT;
		bl->timer.expires = jiffies + TIME_SILENT;
		break;
	case BLINKCODE_STATE_SILENT:
		/* we've finished the period of silence, turn led on */
		led_switch(bl->led, 1);
		bl->state = BLINKCODE_STATE_INIT;
		bl->timer.expires = jiffies + TIME_INIT;
		break;
	case BLINKCODE_STATE_INIT:
		/* we've finished the period of init */
		led_switch(bl->led, 0);
		bl->state = BLINKCODE_STATE_BLINK_OFF;
		bl->timer.expires = jiffies + TIME_BLINK;
		break;
	case BLINKCODE_STATE_BLINK_OFF:
		/* we've been off, turn on */
		led_switch(bl->led, 1);
		bl->state = BLINKCODE_STATE_BLINK_ON;
		bl->num--;
		if (bl->num <= 0) {
			bl->state = BLINKCODE_STATE_NONE;
			return;
		}
		break;
	case BLINKCODE_STATE_BLINK_ON:
		/* we've been on, turn off */
		led_switch(bl->led, 0);
		bl->state = BLINKCODE_STATE_BLINK_OFF;
		break;
	}
	/* default case: re-add the timer */
	timer_add(&bl->timer);
}

void blinkcode_set(int led, enum blinkcode_num num)
{
	if (led >= NUM_LEDS)
		return;

	timer_del(&blink_state[led].timer);

	blink_state[led].num = num;
	blink_state[led].state = BLINKCODE_STATE_NONE;

	if (num != BLINKCODE_NONE)
		timer_add(&blink_state[led].timer);
}

void blinkcode_init(void)
{
	int i;

	for (i = 0; i < NUM_LEDS; i++) {
		blink_state[i].num = 0;
		blink_state[i].state = BLINKCODE_STATE_NONE;
		blink_state[i].led = i+1;
	}
}
