/*
 * common.h
 *
 *  Created on: 27.01.2009
 *      Author: henryk
 */

#ifndef COMMON_H_
#define COMMON_H_

extern void Error(const char* message);
extern void un_braindead_ify_device(int fd);
extern void cleanup(int fd, const char * end_command, unsigned char end_confirmation);

#endif /* COMMON_H_ */
