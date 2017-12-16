// termcolor.h - pretty macros for cute terminal coloring (don't work on Windows < 10)
#ifndef TGBOTLIB_TERMCOLOR_H_
#define TGBOTLIB_TERMCOLOR_H_

#if TGBOTLIB_USECOLORS

#define RED(s)      "\x1b[1;31m" s "\x1b[0m"
#define GREEN(s)    "\x1b[1;32m" s "\x1b[0m"
#define MAGENTA(s)  "\x1b[1;35m" s "\x1b[0m"
#define YELLOW(s)   "\x1b[1;33m" s "\x1b[0m"
#define BLUE(s)     "\x1b[1;34m" s "\x1b[0m"

#else

#define RED(s)      s
#define GREEN(s)    s
#define MAGENTA(s)  s
#define YELLOW(s)   s
#define BLUE(s)     s

#endif // TGBOTLIB_USECOLORS

#endif