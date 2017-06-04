#include <stdio.h>
#include "writefn_data.h"

void writefn_data_init(struct writefn_data *d)
{
	d->ptr = (char*)malloc(1);
	d->sz = 0;
}

int writefn_data_resize(struct writefn_data *d, size_t add_sz)
{
	char *old = d->ptr;
	d->ptr = (char*)realloc(d->ptr, d->sz + add_sz);
	if (!d->ptr) {
		d->ptr = old;
		return 0;
	}
	d->sz += add_sz;
	return d->sz;
}

size_t writefn_data_append(struct writefn_data *d, const char *data, size_t sz)
{
	size_t old_sz = d->sz;

	if (!sz) {
		return 0;
	}
	
	if (!writefn_data_resize(d, sz)) {
		return 0;
	}
	memcpy(d->ptr + old_sz, data, sz);
	return d->sz;
}

void writefn_data_free(struct writefn_data *d)
{
	free(d->ptr);
	d->ptr = 0;
	d->sz = 0;
}

