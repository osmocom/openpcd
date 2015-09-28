#ifndef _PROD_INFO_H
#define _PROD_INFO_H

#define SIMTRACE_VER(a, b, c)	(((a & 0xff) << 16) |	\
				 ((b & 0xff) << 8) |	\
				 ((c & 0xff) << 0))


int prod_info_write(uint32_t ts, uint32_t version, uint32_t reworks);
int prod_info_get(uint32_t *ver, uint32_t *reworks);

#endif
