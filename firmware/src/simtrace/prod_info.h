#ifndef _PROD_INFO_H
#define _PROD_INFO_H

#define SIMTRACE_VER(a, b, c)	(((a & 0xff) << 16) |	\
				 ((b & 0xff) << 8) |	\
				 ((c & 0xff) << 0))


int prod_info_write(u_int32_t ts, u_int32_t version, u_int32_t reworks);
int prod_info_get(u_int32_t *ver, u_int32_t *reworks);

#endif
