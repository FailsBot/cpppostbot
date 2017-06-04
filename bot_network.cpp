#include "writefn_data.h"
#include "termcolor.h"
#include <curl/curl.h>
#include <assert.h>
#include <stdio.h>

size_t json_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	assert(userdata != 0);
	size_t act = size * nmemb;
	writefn_data *data = (writefn_data *)userdata;
	
	if (!writefn_data_append(*data, ptr, act + 1)) {
		fprintf(stderr, RED("json_cb(): writefn_data_append(): out of mem(can't realloc)\n"));
		return 0;
	}
	data->ptr[data->sz - 1]='\0';
	data->sz--;
	return act;
}

CURL *bot_network_init()
{
	CURL *result = curl_easy_init();
	if (!result) {
		return NULL;
	}
	curl_easy_setopt(result, CURLOPT_WRITEFUNCTION, json_cb);
	return result;
}

void bot_network_free(CURL *c)
{
	curl_easy_cleanup(c);
}
