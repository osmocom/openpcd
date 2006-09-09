#ifndef _LED_H
#define _LED_H

extern void led_init(void);
extern void led_switch(int led, int on);
extern int led_get(int led);
extern int led_toggle(int led);

#endif
