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

const char *addAdminCommandName = "addadmin";

// The simplest interface which all command handlers must implement.
class BotCommand {
public:
	virtual bool command(CURL *c, const json &upd, const std::string &cmd, size_t off, TgInteger fromId, TgInteger chatId) = 0;
};

typedef std::vector<std::string> TgUserNamesList;

// The simple users ids list.
class TgUsersList {
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

	IteratorResult begin()
	{
		return postUserIds.begin();
	}

	IteratorResult end()
	{
		return postUserIds.end();
	}
};

class PostHandler {
protected:
	TgUsersList users;
public:
	typedef TgUsersList::IteratorResult It;

	void cancel(CURL *c, TgInteger chatId, TgUsersList::IteratorResult it, const json &upd)
	{
		if (onCancel(c, chatId, it, upd)) {
			stop(it);
		}
	}
	
	void stop(It it)
	{
		users.removeResultId(it);
	}

	bool handleUpdate(CURL *c, TgInteger fromId,
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
						BOT_NAME, COUNTOF(BOT_NAME), nullptr);

			if ((s.find("/cancel") == 0) && my) {
				cancel(c, chatId, it, upd);
				return true;
			}
		}
		return onUpdate(c, fromId, chatId, it, upd);
	}

	void addPostCommand(CURL *c, TgInteger fromId, TgInteger chatId)
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
	TgInteger postChannelId;
public:

	PhotoChannelPostHandler(TgInteger channelId, const char *name = "") : postChannelId(channelId)
	{
		send_request = "Ок, перешли мне фотографию с опциональным описанием";
		sent_successfully = "Фотография успешно отправлена";
		sent_error = "Ошибка отправки фотографии";
		if (*name) {
			send_request += " в канал ";
			send_request += name;

			sent_successfully += " в канал ";
			sent_successfully += name;
			sent_error += " в канал ";
			sent_error += name;
		}
		send_request += ". Или нажми /cancel для отмены.";
		sent_successfully += "!";
		sent_error += ".";
	}

	bool onCancel(CURL *c, TgInteger chatId, TgUsersList::IteratorResult it, const json &upd) override
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

const char *fileadminslist = "adminsnames";
const char *fileadminidslist = "adminsids"; // if the admin has not a @username.

class AddAdminHandler : public PostHandler {
	TgUsersList &h; // the separate list for admin's ids.
	TgUserNamesList &adminNamesList;
	const std::string chanName;

	void addUserToAdminsFromForward(CURL *c, TgInteger chatId, const json &msg)
	{
		auto &fwd = msg["forward_from"];
		TgInteger id = fwd["id"].get<TgInteger>();
		easy_perform_sendMessage(c, chatId,
				"Готово. Пользователь добавлен в список редакторов канала.", TgMessageParse_Normal, 0);
		// easy_perform_sendMessage(c, chatId, (std::string("Ок. пользователь добавлен в список администраторов канала.") + std::to_string(id)).c_str(), 0);
		h.addId(id);
	}

	bool messageIsForward(const json &msg) const
	{
		return msg.find("forward_from") != msg.end()
			&& !msg["forward_from"].is_null();
	}

	// TODO: make a proper username check (like regexp @[A-Za-z_]$).
	bool messageIsAUsername(const std::string &msg) const
	{
		size_t len = msg.length();
		return len != 0 && msg[0] == '@';
	}
public:
	AddAdminHandler(const char *name,
			TgUsersList &hn, TgUserNamesList &l)
		: h(hn), chanName(name), adminNamesList(l) {}

	bool onCancel(CURL *c, TgInteger chatId, It it, const json &msg) override
	{
		if (messageIsForward(msg)) {
			addUserToAdminsFromForward(c, chatId, msg);
			return true;
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
			// easy_perform_sendMessage(c, chatId, "вжух", TgMessageParse_Normal, 0);
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

class BotCommandsHandler {
	std::map<std::string, std::unique_ptr<BotCommand> > commands;

public:
	
	bool handleCommands(CURL *c, TgInteger fromId,
			TgInteger chatId, const json &upd)
	{
		auto text  = upd["text"].get<std::string>();
		size_t off = 0;
		if (!easy_bot_check_command(text.c_str(), text.length(),
					BOT_NAME, COUNTOF(BOT_NAME), &off)) {
			return false;
		}

		std::string s2 = text.substr(1, off - 1);
		printf("\ntest found cmd: %s\n", s2.c_str());
		const auto &cmd = commands.find(s2);
		if (cmd == commands.end()) {
			return false;
		}
		cmd->second->command(c, upd, text, off, fromId, chatId);
		
		return true;
	}

	void addCommand(const std::string &name, std::unique_ptr<BotCommand> command)
	{
		commands[name].swap(command);
	}
};

class ResponseBotCommand : public BotCommand {
	std::string message;
	TgMessageParseMode mode;
	std::string additional;
public:
	ResponseBotCommand(std::string msg, TgMessageParseMode mode = TgMessageParse_Normal, std::string additional = "")
		: message(std::move(msg)), mode(mode), additional(additional) {}

	virtual bool command(CURL *c, const json &upd, const std::string &cmd, size_t off, TgInteger fromId, TgInteger chatId) override
	{
		easy_perform_sendMessage(c, chatId, message.c_str(),
				mode, 0, additional.c_str(), 0);
		return true;
	}
};

class PostCommandHandler : public BotCommand {
	PhotoChannelPostHandler &h;
public:
	PostCommandHandler(PhotoChannelPostHandler &ph) : h(ph) {}

	virtual bool command(CURL *c, const json &upd, const std::string &cmd, size_t off, TgInteger fromId, TgInteger chatId) override
	{
		h.addPostCommand(c, fromId, chatId); 

		return true;
	}
};

class AddAdminCommand : public BotCommand {
	AddAdminHandler &h;
public:
	AddAdminCommand(AddAdminHandler &hn) : h(hn) {}

	virtual bool command(CURL *c, const json &upd, const std::string &cmd, size_t off, TgInteger fromId, TgInteger chatId) override
	{
		if (fromId != idOwner) {
			return false;
		}
		h.addPostCommand(c, fromId, chatId);
		return true;
	}

};

class PrintUserListCommand : public BotCommand {
	TgUsersList &users;
	TgUserNamesList &usernames;
public:
	PrintUserListCommand(TgUsersList &u, TgUserNamesList &l) : users(u), usernames(l) {}

	virtual bool command(CURL *c, const json &upd, const std::string &cmd, size_t off, TgInteger fromId, TgInteger chatId) override
	{
		if (fromId != idOwner) {
			return false;
		}
		std::ostringstream oss;
		oss << "Список редакторов:\n";
		size_t i = 0;
		for (auto it : usernames) {
			oss << (i + 1) << ". " << it << ";\n";
			i++;
		}
		for (auto it : users) {
			oss << (i + 1) << ". id" << it << ";\n";
			i++;
		}
		if (i == 0) {
			oss << "(Список пустой. Добавьте нового с помощью команды /" << addAdminCommandName << ".)";
		}
		std::string s2 = oss.str();
		const char *s3 = curl_easy_escape(c, s2.c_str(), s2.length());
		easy_perform_sendMessage(c, chatId, s3, TgMessageParse_Normal, 0, 0, 0);
		curl_free( (char*)s3);
		return true;

	}
};

// Bot global context
TgUsersList adminIdsList;
std::vector<std::string> adminNamesList;
PhotoChannelPostHandler photoPostHandler(idPostChannel, postChannelName);
BotCommandsHandler commandsHandler;
AddAdminHandler addAdminsHandler(postChannelName, adminIdsList, adminNamesList);

void handle_update_message(CURL *c, json &res, bool &quit, size_t &updId)
{
	TgInteger fromId = 0;
	TgInteger chatId = 0;
	TgInteger updId2  = res["update_id"].get<TgInteger>();
	auto &msg = res["message"];
	auto &from = msg["from"];
	auto &chat = msg["chat"];
	fromId = from["id"].get<TgInteger>();
	chatId = chat["id"].get<TgInteger>();

	updId = updId2 + 1;
}

void handle_all_updates(CURL *c, json &upd, bool &quit, size_t &updId)
{
	auto &r = upd["result"];
	if (!r.is_array()) {
		quit = true;
		return;
	}
	
	for (auto res : r) {
		handle_update_message(c, res, quit, updId);
	}
}

int main(int argc, char *argv[])
{
	size_t upd_id = 0;
	size_t sleep_time = 10;
	writefn_data d;
	CURL *c = bot_network_init();
	json upd;
	bool quit = false;
	commandsHandler.addCommand(postCommandName,
			std::make_unique<PostCommandHandler>(photoPostHandler));
	commandsHandler.addCommand("start",
			std::make_unique<ResponseBotCommand>(
				std::string("Привет! Я @" BOT_NAME "! Я могу постить твои фотографии с подписями в канал. Чтобы это сделать, используй команду /") + postCommandName, TgMessageParse_Normal));
	commandsHandler.addCommand("addadmin",
			std::make_unique<AddAdminCommand>(addAdminsHandler));
	commandsHandler.addCommand("adminlist",
			std::make_unique<PrintUserListCommand>(adminIdsList, adminNamesList));

	do {
		writefn_data_init(d);
		if(easy_perform_getUpdates(c, &d, sleep_time, upd_id) != CURLE_OK) {
			fprintf(stderr, "Bot network error.\n");
			break;
		} 
		if (easy_get_http_code(c) != 200) {
			printf("%s\n", d.ptr);
			break;
		}
		printf("%s\n", d.ptr);
		upd = json::parse(d.ptr);
		if (upd["ok"].is_null() || !upd["ok"].is_boolean() || !(upd["ok"].get<bool>())) {
			fprintf(stderr, "Telegram server returns non-ok result: %s\n", d.ptr);
			break;
		}
		handle_all_updates(c, upd, quit, upd_id);
		writefn_data_free(d);
	} while (!quit);
	if (!quit) {
		writefn_data_free(d);
	}
	bot_network_free(c);
	return 0;
}
