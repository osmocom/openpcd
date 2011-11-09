#ifndef _ASM_COMPILER_H
#define _ASM_COMPILER_H

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define __unused	__attribute__((unused))
#define __noreturn	__attribute__((noreturn))

#endif
