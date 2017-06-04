#ifndef _TGBOTLIB_WRITEFN_DATA_H_
#define _TGBOTLIB_WRITEFN_DATA_H_

#include <stddef.h>

struct writefn_data {
	char *ptr;
	size_t sz;
};

#ifdef __cplusplus
extern "C" {
#endif

void writefn_data_init(struct writefn_data *d);
void writefn_data_free(struct writefn_data *d);
size_t writefn_data_append(struct writefn_data *d, const char *data, size_t sz);

#ifdef __cplusplus
}

inline void writefn_data_init(writefn_data &d)
{
	writefn_data_init(&d);
}

inline void writefn_data_free(writefn_data &d)
{
	writefn_data_free(&d);
}

inline size_t writefn_data_append(writefn_data &d, const char *data, size_t sz)
{
	return writefn_data_append(&d, data, sz);
}

#endif

#endif
