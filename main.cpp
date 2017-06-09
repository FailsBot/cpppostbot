#include "bot_easy_api.h"
#include "writefn_data.h"

#include <vector>
#include <algorithm>
#include <memory>
#include "to_string.h"
#include "json.hpp"
#include "botkey.h"
#include "const_str.h"

using nlohmann::json;

CURL *bot_network_init();
void bot_network_free(CURL *c);

// Channel config 
const TgInteger idPostChannel =
#include "cfg/channelid"
;
const char *postChannelName = 
#include "cfg/channelname"
;

// Bot commands config
const TgInteger idOwner =
#include "cfg/ownerid"
;
const char *postCommandName =
#include "cfg/postcommandname"
;

class UpdateStateHandler {
	std::vector<TgInteger> postUserIds;
public:
	typedef std::vector<TgInteger>::iterator IteratorResult;

	bool haveId(TgInteger userId)
	{
		return findId(userId) != postUserIds.end(); 
	}

	bool haveResultId(IteratorResult r)
	{
		return r != postUserIds.end();
	}

	IteratorResult findId(TgInteger userId)
	{
		return std::find(postUserIds.begin(), postUserIds.end(), userId);	
	}

	bool addId(TgInteger id)
	{
		postUserIds.push_back(id);
		return true;
	}

	bool removeResultId(IteratorResult id)
	{
		postUserIds.erase(id);
		return true;
	}

};

class PostHandler {
protected:
	UpdateStateHandler users;
	TgInteger postChannelId;
public:
	typedef UpdateStateHandler::IteratorResult It;

	PostHandler(TgInteger chatId)
		: postChannelId(chatId) {}

	void cancel(CURL *c, TgInteger chatId, UpdateStateHandler::IteratorResult it, const json &upd)
	{
		if (onCancel(c, chatId, it, upd)) {
			stop(it);
		}
	}
	
	void stop(It it)
	{
		users.removeResultId(it);
	}

	bool handle_private_updates(CURL *c, TgInteger fromId,
			TgInteger chatId, const json &upd)
	{
		auto it = users.findId(fromId);
		if (!users.haveResultId(it)) {
			return false;
		}

		const auto & it2 = upd.find("text");
		bool have_t = it2 != upd.end() && !it2->is_null();

		if (have_t) {
			const std::string &s = upd["text"].get<std::string>();
			bool my = easy_bot_check_command(s.c_str(), s.length(),
						"FailsBot", 8, nullptr);

			if ((s.find("/cancel") == 0) && my) {
				cancel(c, chatId, it, upd);
				return true;
			}
		}
		return onUpdate(c, fromId, chatId, it, upd);
	}

	void handle_post_command(CURL *c, TgInteger fromId, TgInteger chatId)
	{
		if (onAddHandler(c, fromId, chatId)) {
			users.addId(fromId);
		}
	}
	virtual ~PostHandler() {}
protected:
	virtual bool onCancel(CURL *c, TgInteger chatId, It it, const json &upd) = 0;
	virtual bool onAddHandler(CURL *c, TgInteger chatId, TgInteger fromId) = 0;
	virtual bool onUpdate(CURL *c, TgInteger chatId, TgInteger fromId, It it, const json &upd) = 0;

};


class PhotoChannelPostHandler : public PostHandler {
	std::string send_request;
	std::string sent_successfully;
	std::string sent_error;
public:

	PhotoChannelPostHandler(TgInteger channelId, const char *name = "") : PostHandler(channelId)
	{
		send_request = "Ок, перешли мне фотографию с опциональным описанием";
		sent_successfully = "Фотография успешно отправлена";
		sent_error = "Ошибка отправки фотографии";
		if (*name) {
			send_request += " в канал ";
			send_request += name;

			sent_successfully += " в канал ";
			sent_successfully += name;
			sent_error += "в канал ";
			sent_error += name;
		}
		send_request += ". Или нажми /cancel для отмены.";
		sent_successfully += "!";
		sent_error += ".";
	}

	bool onCancel(CURL *c, TgInteger chatId, UpdateStateHandler::IteratorResult it, const json &upd) override
	{
		 easy_perform_sendMessage(c, chatId, "Ок, команда отменена.", TgMessageParse_Normal, 0);
		return true;
	}

	bool onAddHandler(CURL *c, TgInteger chatId, TgInteger fromId) override
	{
		easy_perform_sendMessage(c, chatId, send_request.c_str(), 
				TgMessageParse_Normal, 0);
		return true;
	}

	bool onUpdate(CURL *c, TgInteger chatId, TgInteger fromId, It it, const json &upd) override
	{
		 if (upd.find("photo") != upd.end()) {
			TgInteger messageId = upd["message_id"].get<TgInteger>();
			int result = easy_perform_forwardMessage(c, postChannelId, messageId,
					chatId);
			easy_perform_sendMessage(c, chatId, result == 200 ? sent_successfully.c_str() : sent_error.c_str(), TgMessageParse_Html, 0);
			stop(it);
			return true;
		}
		 return false;
	}
};

class AddAdminHandler : public PostHandler {
	UpdateStateHandler &h;

	void addUserToAdminsFromForward(CURL *c, TgInteger chatId, const json &msg)
	{
		auto &fwd = msg["forward_from"];
		TgInteger id = fwd["id"].get<TgInteger>();
		easy_perform_sendMessage(c, chatId,
				"Этот пользователь добавлен в список редакторов канала.", TgMessageParse_Normal, 0);
		// easy_perform_sendMessage(c, chatId, (std::string("Ок. пользователь добавлен в список администраторов канала.") + std::to_string(id)).c_str(), 0);
		h.addId(id);
	}

	bool messageIsForward(const json &msg) const
	{
		return msg.find("forward_from") != msg.end()
			&& !msg["forward_from"].is_null();
	}
public:
	AddAdminHandler(TgInteger channelId, const char *name,
			UpdateStateHandler &hn)
		: PostHandler(channelId), h(hn) {}

	bool onCancel(CURL *c, TgInteger chatId, It it, const json &msg) override
	{
		if (messageIsForward(msg)) {
			addUserToAdminsFromForward(c, chatId, msg);
		}
		easy_perform_sendMessage(c, chatId, "Ок, команда отменена.", TgMessageParse_Normal, 0);
		return true;
	}

	bool onUpdate(CURL *c, TgInteger fromId,
			TgInteger chatId, It it, const json &upd) override
	{
		bool has_text = upd["text"].is_string();

		if (!has_text) {
			return true;
		}

		TgInteger id = 0;
		bool is_fwd = messageIsForward(upd);
		if (is_fwd) {
			addUserToAdminsFromForward(c, chatId, upd);
			stop(it);
			return true;
		} else {

			// todo, bleat.
			stop(it);
			easy_perform_sendMessage(c, chatId, "вжух", TgMessageParse_Normal, 0);
		}
		return true;
	}

	bool onAddHandler(CURL *c, TgInteger fromId,
			TgInteger chatId) override
	{
		easy_perform_sendMessage(c, chatId, "Ок, отправь мне форвард от того пользователя, которого ты хочешь сделать редактором канала.", TgMessageParse_Normal, 0);
		return true;
	}
};

class BotCommand {
public:
	virtual bool command(CURL *c, const json &upd, const std::string &cmd, size_t off) = 0;
};

class BotCommandsHandler {
	std::map<std::string, std::unique_ptr<BotCommand> > commands;

public:
	
	bool handleCommands(CURL *c, TgInteger fromId,
			TgInteger chatId, const json &upd)
	{
		auto text  = upd["message"].get<std::string>();
		const auto &it = commands.find("vzhuh");
		size_t off = 0;
		if (!easy_bot_check_command(text.c_str(), text.length(),
					BOT_NAME, COUNTOF(BOT_NAME), &off)) {
			return false;
		}
		
		return true;
	}

	void addCommand(const std::string &name, std::unique_ptr<BotCommand> command)
	{
		commands[name].swap(command);
	}
};

class PostCommandHandler : public BotCommand {
	PhotoChannelPostHandler &h;
public:
	PostCommandHandler(PhotoChannelPostHandler &ph) : h(ph) {}

	bool command(CURL *c, const json &upd, const std::string &cmd, size_t off) override
	{

		return true;
	}
};

PhotoChannelPostHandler photoPostHandler(idPostChannel, postChannelName);
BotCommandsHandler commandsHandler;

void handle_update_message(CURL *c, json &msg, bool &quit, size_t &updId)
{
	TgInteger fromId = 0;
	TgInteger chatId = 0;
	auto &from = msg["from"];
	auto &chat = msg["chat"];
	fromId = from["id"].get<TgInteger>();
	chatId = chat["id"].get<TgInteger>();
	photoPostHandler.handle_private_updates(c, fromId, chatId, msg);
	commandsHandler.handleCommands(c, fromId, chatId, msg);
}

void handle_all_updates(CURL *c, json &upd, bool &quit, size_t &updId)
{
	auto &r = upd["result"];
	if (!r.is_array()) {
		quit = true;
		return;
	}
	
	for (auto msg : r) {
		handle_all_updates(c, msg, quit, updId);
	}
}

int main(int argc, char *argv[])
{
	size_t upd_id = 0;
	size_t sleep_time = 0;
	writefn_data d;
	writefn_data_init(d);
	CURL *c = bot_network_init();
	json upd;
	bool quit = false;
	commandsHandler.addCommand(postCommandName,
			std::make_unique<PostCommandHandler>(photoPostHandler));
	do {
		if(easy_perform_getUpdates(c, &d, upd_id, sleep_time) != CURLE_OK) {
			fprintf(stderr, "Bot network error.\n");
			break;
		} 
		if (easy_get_http_code(c) != 200) {
			printf("%s\n", d.ptr);
			break;
		}
		upd = json::parse(d.ptr);
		if (upd["ok"].is_null() || !upd["ok"].is_boolean() || !(upd["ok"].get<bool>())) {
			fprintf(stderr, "Telegram server returns non-ok result: %s\n", d.ptr);
			break;
		}
		handle_all_updates(c, upd, quit, upd_id);
	} while (!quit);
	bot_network_free(c);
	return 0;
}
