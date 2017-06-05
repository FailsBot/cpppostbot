#include <curl/curl.h>
#include "to_string.h" // work around fucked android ndk bug
#include <stdio.h>
#include <string>
#include "writefn_data.h"
#include "botkey.h"
#include "termcolor.h"
#include "tgtypes.h"
#include <assert.h>

//
// Bot easy API
//

int easy_get_http_code(CURL *c)
{
	int st = 0;
	int st2 = curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &st);
	return st2 == CURLE_OK ? st : - st2;
}

/// @brief Prints HTTP code and response. 
void easy_print_http_code(CURL *c, writefn_data *d = 0)
{
	int status = 0;
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
	printf("HTTP status: %s(%d)\n",
		(status < 400) ? GREEN("OK") : RED("FAILED"), status);
	if (!d) {
		return;
	}
	printf("%s\n", d->ptr);
}

int easy_perform_commandstr(CURL *c, const char *url, writefn_data *data, 
	bool print_result = true)
{
	assert(data);
	printf("Perform command %s\n", url);
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, data);
	CURLcode result = curl_easy_perform(c);
	if (result != CURLE_OK) {
		long code = 0;
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
		fprintf(stderr, RED("Failed to answer the user: ")  
			 "HTTP status " RED("%ld") ", curl err code: %d"
			 ", description: %s\n", code, result, curl_easy_strerror(result));
		// return result;
		return -result;
	}

	if (print_result) {
		easy_print_http_code(c);
	}
	return result;
}

int easy_perform_commandstr(CURL *c, const char *url)
{
	writefn_data d;
	writefn_data_init(d);
	int result = easy_perform_commandstr(c, url, &d);
	writefn_data_free(d);
	return result;
}

int easy_perform_getUpdates(CURL *c, writefn_data *d, size_t poll_time = 0, size_t update_offset = 0)
{
	std::string s =  BOT_URL "getUpdates";
	bool que = false;
	if (update_offset != 0) {
		que = true;
		s += "?offset=" + std::to_string(update_offset);
	}
	if (poll_time != 0) {
		s += (que ? "&timeout=": "?timeout=") + std::to_string(poll_time);
	}
	int r = easy_perform_commandstr(c, s.c_str(), d);
	return r;
}

int easy_perform_getUpdates_auto(CURL *c)
{
	writefn_data d;
	writefn_data_init(d);
	int r = easy_perform_getUpdates(c, &d);
	easy_print_http_code(c);
	writefn_data_free(d);
	return r;
}

int easy_perform_sendMessage(CURL *c, const char *chat_id, 
	const char *msg, TgMessageParseMode mode, TgInteger reply_id,
	const char *additional = 0, writefn_data *d2 = 0)
{
	static const char *modes[] = {
		"Markdown",
		"HTML",
	};

	std::string query = BOT_URL "sendMessage";
	query += "?chat_id=" + std::string(chat_id) + "&text=" + std::string(msg);
	writefn_data d;
	writefn_data_init(d);
	if (mode != TgMessageParse_Normal) {
		query += "&parse_mode=";
		query += modes[mode - 1];
	}
	if (additional) {
		query += additional;
	}
	if (reply_id) {
		query += "&reply_to_message_id=" + std::to_string(reply_id);
	}
	int r = easy_perform_commandstr(c, query.c_str(), &d);
	if (d2 == 0) {
		writefn_data_free(d);
	} else {
		*d2 = d;
	}
	return r;
}

int easy_perform_sendMessage(CURL *c, TgInteger chat_id, 
	const char *msg, TgMessageParseMode mode, TgInteger reply_id,
	const char *additional = 0, writefn_data *d2 = 0)
{
	return easy_perform_sendMessage(c, std::to_string(chat_id).c_str(),
			msg, mode, reply_id, additional, d2);
}

int easy_perform_sendMessage_s(CURL *c, const char *chat_id,
		const char *msg, bool markdown,
		const char *additional = 0, TgInteger reply_id = 0, 
		writefn_data *d2 = 0)
{
	return easy_perform_sendMessage(c, chat_id, msg,
			markdown ? TgMessageParse_Markdown :
			TgMessageParse_Normal, reply_id, additional, d2);
}

int easy_perform_sendMessage(CURL *c, TgInteger chat_id, const char *msg,
		bool markdown, const char *additional = 0,
		TgInteger reply_id = 0,
		writefn_data *d = 0)
{
	return easy_perform_sendMessage_s(c, std::to_string(chat_id).c_str(),
			msg, markdown, additional, reply_id, d);	
}

int easy_perform_sendChatAction(CURL *c, TgInteger chat_id,
		const char *action)
{
	std::string s = BOT_URL "sendChatAction?chat_id=" +
		std::to_string(chat_id) + "&action=" + 
		std::string(action);
	writefn_data d;
	writefn_data_init(d);
	int res = easy_perform_commandstr(c, s.c_str(), &d);
	printf("%s\n",d.ptr);
	writefn_data_free(d);
	return res;
}

int easy_perform_sendSticker(CURL *c, TgInteger chat_id, const char *sticker_id,
	TgInteger reply_id = 0,	const char *additional = 0)
{
	writefn_data d;
	writefn_data_init(d);
	std::string query = BOT_URL "sendSticker";
	query += "?chat_id=" + std::to_string(chat_id) + "&sticker=";
	query += sticker_id;
	if (additional) {
		query += additional;
	}
	if (reply_id) {
		query += "&reply_to_message_id=" + std::to_string(reply_id);
	}
	int r = easy_perform_commandstr(c, query.c_str(), &d); 
	writefn_data_free(d);
	return r;
}

int easy_perform_sendLocation(CURL *c, TgInteger chat_id,
		const TgLocation &loc,
		TgInteger reply_id = 0, writefn_data *d = 0)
{
	std::string s = BOT_URL + std::string("sendLocation?latitude=") + 
		std::to_string(loc.latitude) + "&longitude=" +
		std::to_string(loc.longitude) + "&chat_id=" +
		std::to_string(chat_id);
	if (reply_id) {
		s += "&reply_to_message_id=" + std::to_string(reply_id);
	}
	return easy_perform_commandstr(c, s.c_str(), d);
	// return 0;
}

int easy_perform_sendVenue(CURL *c, TgInteger chat_id,
		const TgLocation &loc,
		const char *title, const char *address,
		TgInteger reply_id = 0, writefn_data *d = 0)
{
	std::string s = BOT_URL "sendVenue";
	s += std::string("?chat_id=") + std::to_string(chat_id);
	char *t = curl_easy_escape(c, title, 0);
	char *a = curl_easy_escape(c, address, 0);
	s += std::string("&title=") + std::string(t) + "&address="
		+ std::string(a) + "&latitude="
		+ std::to_string(loc.latitude) + "&longitude="
		+ std::to_string(loc.longitude);
	curl_free(t);
	curl_free(a);
	if (reply_id) {
		s += "&reply_to_message_id=" + std::to_string(reply_id);
	}
	return easy_perform_commandstr(c, s.c_str(), d);
}

int easy_perform_leaveChat(CURL *c, TgInteger chatId)
{
	return easy_perform_commandstr(c, 
			(std::string(BOT_URL "leaveChat?chat_id=")+std::to_string(chatId)).c_str());
}

int easy_perform_forwardMessage_(CURL *c, TgInteger chatId,
		TgInteger messageId, const char *channel)//,
	//	writefn_data *d = 0)
{
	writefn_data data;
	writefn_data_init(data);

	std::string s = BOT_URL "forwardMessage?from_chat_id="
		 + std::string(channel) + "&chat_id="
		+ std::to_string(chatId)
		+ "&message_id=" + std::to_string(messageId);
	int res = easy_perform_commandstr(c, s.c_str(), &data);
	printf("Result: %s", data.ptr);
	writefn_data_free(data);
	if (res != CURLE_OK) {
		return -res;
	}
	return easy_get_http_code(c);
}

int easy_perform_forwardMessage(CURL *c, TgInteger chatId,
		TgInteger messageId, const char *channel)
{
	std::string ch = "@";
	ch += channel;
	return easy_perform_forwardMessage_(c, chatId, messageId,
			ch.c_str());
}

int easy_perform_forwardMessage(CURL *c, TgInteger chatId,
		TgInteger messageId, TgInteger fromChatId)
{
	return easy_perform_forwardMessage_(c, chatId, messageId,
			std::to_string(fromChatId).c_str());
}

int easy_perform_deleteMessage(CURL *c, TgInteger chatId,
		TgInteger messageId)
{
	easy_perform_commandstr(c, (std::string(BOT_URL)
				+ "deleteMessage?chat_id=" + std::to_string(chatId)
				+ "&message_id=" + std::to_string(messageId)).c_str()  );
	return 0;
}


int easy_perform_deleteMessage(CURL *c, const char *groupName,
		TgInteger messageId)
{
	return 1; // easy_perform_deleteMessage(c, , messageId);	
}

int easy_perform_sendGame(CURL *c, TgInteger chat_id,
		const char *game_name, TgInteger reply_id,
		const char *additional, writefn_data *d = 0)
{
	std::string query = BOT_URL "sendGame?chat_id=" +
		std::to_string(chat_id) + "&game_short_name=" +
		std::string(game_name);
	if (reply_id) {
		query += "&reply_to_message=" + std::to_string(reply_id);
	}
	if (additional) {
		query += additional;
	}
	int res = easy_perform_commandstr(c, query.c_str(), d);
	return res == CURLE_OK ? easy_get_http_code(c) : -res;
}

//
// chunked msg
//

bool ishexnum(int c)
{
	return  (c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}

bool find_percent_enc(char *buf, size_t &cur_off, size_t max_offset)
{
	size_t old_off = cur_off;
	while (ishexnum(buf[cur_off])) {
		if (cur_off == 0) {
			cur_off = old_off;
			return false;
		}
		cur_off--;
	}
	if (buf[cur_off] == '%') {
		return true;
	}
	return false;
}

typedef bool (*chunk_perform_callback)(const char *buf, size_t sz,
		void *param);

bool easy_perform_chunked_message(char *buf, size_t bufsz,
		const size_t MAX_MSG_SIZE, chunk_perform_callback cb,
		void *param)
{
	char *chunk = buf;
	char old;
	size_t counter = MAX_MSG_SIZE;
	if (bufsz < counter) {
		cb(buf, bufsz, param);
		return true;
	}
	for (size_t i = 0; i < bufsz; i += counter, bufsz -= counter) {
		counter = MAX_MSG_SIZE;
		
		if (!find_percent_enc(chunk, counter, MAX_MSG_SIZE)) {
			// assert(0);
		}
		old = chunk[counter];
		chunk [counter] = '\0';
		cb(chunk, bufsz, param);
		chunk [counter] = old;
		chunk += counter;
	}
	cb(chunk, bufsz, param);
	return true;
}

int easy_perform_sendEscapedLongMessage(CURL *curl, TgInteger chatId,
		const char *msg, size_t length,
		TgMessageParseMode parseMode, TgInteger replyId,
		const char *additional, writefn_data *d)
{
	char *a = curl_easy_escape(curl, msg, length);
	struct sndmsg_chunk_ctx {
		CURL *c;
		TgInteger chat;
		TgMessageParseMode mode;
		TgInteger reply_id;
		const char *additional;
		writefn_data *d;
		static bool snd(const char *buf,
				size_t sz, void *p){
			sndmsg_chunk_ctx *c = 
				(sndmsg_chunk_ctx*)p;
		easy_perform_sendMessage(c->c, c->chat, buf, c->mode,
				c->reply_id, c->additional, c->d);
			return true;
		} 
	} c = {
		curl,
		chatId,
		parseMode,
		replyId,
		additional,
		d
	};
	int result = easy_perform_chunked_message(a, strlen(a),
		4096, c.snd, &c);
	curl_free(a);
	return result;
}

bool easy_bot_check_command(const char *cmd, size_t sz, const char *name, 
		size_t name_sz, size_t *cmd_end_off, bool *shortcmd = 0)
{
	if (cmd[0] != '/') {
		return false;
	}
	const char *tok = strpbrk(cmd + 1, "@ ");
	if (!tok) {
		if (cmd_end_off) {
			*cmd_end_off = sz;
		}
		if (shortcmd) {
			*shortcmd = true;
		}
		return true;
	}
	switch (*tok) {
	case '@':
		if (strncmp(tok + 1, name, name_sz) == 0) {
			if (cmd_end_off) {
				*cmd_end_off = tok - cmd;
			}
			if (shortcmd) {
				*shortcmd = false;
			}
			return true;
		}
		return false;
	case ' ':
		if (cmd_end_off) {
			*cmd_end_off = tok - cmd;
		}
		if (shortcmd) {
			*shortcmd = true;
		}
		return true;
	}
	return true;
}

#if 0
static void test_cmd(CURL *c, TgInteger chatId)
{
	struct {
		const_string cmd;
		bool ret, shrt;
		size_t off;
	} expected[] = {
		{ MAKE_CONST_STR("huita"), false, false, 0 },
		{ MAKE_CONST_STR("/h@bot2"), false, false, 0 },
		{ MAKE_CONST_STR("/h@bot2 h2"), false, false, 0 },
		{ MAKE_CONST_STR("/h"), true, true, 2 },
		{ MAKE_CONST_STR("/h h2"), true, true, 2 },
		{ MAKE_CONST_STR("/h@bot"), true, false, 6 },
		{ MAKE_CONST_STR("/h@bot h2"), true, false, 6 } 
	};
	const_string name = MAKE_CONST_STR("bot");
	size_t off = 0;
	bool shrt = 0;
	bool r = 0;
	std::string msg = "Test suite.\n";
	for (size_t i = 0; i < COUNTOF(expected); i++) {
		msg += "Test #*" + std::to_string(i) + "*. Test string: `"
			+ std::string(expected[i].cmd.str) + "`, expected retval: " + std::string(expected[i].ret ? "true" : "false") ;
		if (expected[i].ret) {
			msg += ", shortflag: " + std::string(expected[i].shrt ? "true" : "false") + ", offset: " + std::to_string(expected[i].off);
		}
		bool r = easy_bot_check_command(expected[i].cmd.str, expected[i].cmd.len,
				name.str, name.len, &off, &shrt);
		msg += ", actual: retval: " + ((expected[i].ret == r) ? 
			std::string(r ? "true" : "false") :
			std::string(r ? "*TRUE*" : "*FALSE*"));
		if (r) {
			msg += ", shortflag: " + ((expected[i].shrt == shrt) ? 
			std::string(shrt ? "true" : "false") :
			std::string(shrt ? "*TRUE*" : "*FALSE*"));
			
			bool boldoff = off != expected[i].off;
			msg += ", off: ";
			if (boldoff) {
				msg += "*";
			}
			msg += std::to_string(off);

			if (boldoff) {
				msg += "*";
			}
		}
		msg += "\n";
	}
	char *s = curl_easy_escape(c, msg.c_str(), msg.length());
	easy_perform_sendMessage(c, chatId, s, true);
	curl_free(s);
}
#endif
