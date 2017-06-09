// const_str.h - contains a declaration of token helper struct and macros.
// 
#ifndef TGBOTLIB_CONST_STR_H
#define TGBOTLIB_CONST_STR_H
#include <stddef.h>

struct const_str {
	char *str;
	size_t sz;
};

#define COUNTOF(str) (sizeof(str) / sizeof(str[0]))
#define MAKE_CONST_STR(str), { (str), COUNTOF(str) }

#endif
