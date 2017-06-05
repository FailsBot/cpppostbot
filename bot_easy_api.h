#ifndef TGBOTLIB_EASY_API_H
#define TGBOTLIB_EASY_API_H
#include "tgtypes.h"

// struct CURL;
#include <curl/curl.h>
struct writefn_data;


int easy_get_http_code(CURL *c);
void easy_print_http_code(CURL *c, writefn_data *d = 0);
int easy_perform_commandstr(CURL *c, const char *url, writefn_data *data, 
	bool print_result = true);
int easy_perform_commandstr(CURL *c, const char *url);

int easy_perform_getUpdates(CURL *c, writefn_data *d,
		size_t poll_time = 0, size_t update_offset = 0);
int easy_perform_sendMessage(CURL *c, const char *chat_id, 
	const char *msg, TgMessageParseMode mode, TgInteger reply_id,
	const char *additional = 0, writefn_data *d2 = 0);
int easy_perform_sendMessage(CURL *c, TgInteger chat_id, 
	const char *msg, TgMessageParseMode mode, TgInteger reply_id,
	const char *additional = 0, writefn_data *d2 = 0);

int easy_perform_forwardMessage(CURL *c, TgInteger chatId,
		TgInteger messageId, TgInteger fromChatId);
int easy_perform_deleteMessage(CURL *c, TgInteger chatId,
		TgInteger messageId);
int easy_perform_sendLocation(CURL *c, TgInteger chat_id,
		const TgLocation &loc,
		TgInteger reply_id = 0, writefn_data *d = 0);
int easy_perform_leaveChat(CURL *c, TgInteger chatId);
bool easy_bot_check_command(const char *cmd, size_t sz, const char *name, 
		size_t name_sz, size_t *cmd_end_off, bool *shortcmd = 0);
#endif
