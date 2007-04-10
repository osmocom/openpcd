#ifndef _USBAPI_GENERIC_H
#define _USBAPI_GENERIC_H
extern void usbcmd_gen_init(void);
extern int gen_setenv(void* data,u_int32_t pos,u_int32_t length);
extern int gen_getenv(void* data,u_int32_t pos,u_int32_t length);
#endif
