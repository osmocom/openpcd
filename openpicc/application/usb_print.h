#ifndef USB_PRINT_H_
#define USB_PRINT_H_

extern void usb_print_buffer(const char* buffer, int start, int stop);
extern int usb_print_buffer_f(const char* buffer, int start, int stop, int flush);
extern void usb_print_string(const char *string);
extern int usb_print_string_f(const char* string, int flush);
extern void usb_print_char(const char c);
extern int usb_print_char_f(const char c, int flush);
extern void usb_print_flush(void);
extern int usb_print_get_default_flush(void);
extern int usb_print_set_default_flush(int flush);
extern int usb_print_set_force_silence(int silence);
extern void usb_print_init(void);

#endif /*USB_PRINT_H_*/
