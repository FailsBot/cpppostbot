#include "bot_easy_api.h"
#include "writefn_data.h"

#include <vector>
#include <algorithm>
#include <memory>
#include <fstream>
#include <signal.h>

#ifdef USE_TO_STRING_HACK
#include "to_string.h"
#endif

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

const size_t adminsListFlushIntervalInUpdCalls =
#include "cfg/admlistflushinterval"
;

// Another constants.

const char *addAdminCommandName = "addadmin";

// Persistent storage config
const char *fileadminslist = "adminsnames";
const char *fileadminidslist = "adminsids"; // if the admin has not a @username.

// The simplest interface which all command handlers must implement.
class BotCommand {
public:
	virtual bool command(CURL *c, const json &upd, const std::string &cmd, size_t off, TgInteger fromId, TgInteger chatId) = 0;
};

// The interface for all update handlers.
class UpdateHandler {
public:

	virtual bool handleUpdate(CURL *c, TgInteger fromId,
			TgInteger chatId, const json &upd) = 0;
	virtual ~UpdateHandler() {}
};

typedef std::vector<std::string> TgUserNamesList;

// The simple users ids list.
// XXX: transform it to simple typedef for std::vector
// for generic template functions like loadFromFile().
class TgUsersList {
	std::vector<TgInteger> postUserIds;
public:
	typedef std::vector<TgInteger>::iterator IteratorResult;

	std::vector<TgInteger> &vec() 
	{
		return postUserIds;
	}

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

class PostHandler : public UpdateHandler {
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

class ModifyAdminsHandler : public PostHandler {
protected:
	TgUsersList &h; // the separate list for admin's ids.
	TgUserNamesList &adminNamesList;
	const std::string chanName;

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

	virtual bool handleForward(CURL *c, TgInteger chatId, It currentUserId, const json &msg, bool fromCancel) = 0;
	virtual bool handleMessage(CURL *c, TgInteger chatId, It currentUserId, const json &msg) = 0;

public:
	class ModifyEntriesListener {
	public:
		enum NotifyType {
			ModifyAdminName,
			ModifyAdminId
		};

		virtual void notify(NotifyType type) = 0;
		virtual ~ModifyEntriesListener() {}
	};
protected:
	void notifyListener(ModifyEntriesListener::NotifyType type)
	{
		if (listener) {
			listener->notify(type);
		}
	}
private:
	class ModifyEntriesListener *listener;
public:
	void addListener(ModifyEntriesListener &l)
	{
		listener = &l;
	}

	void removeListener()
	{
		listener = nullptr;
	}

	ModifyAdminsHandler(const char *name,
			TgUsersList &hn, TgUserNamesList &l)
		: h(hn), chanName(name), adminNamesList(l) {}

	bool onCancel(CURL *c, TgInteger chatId, It it, const json &msg) override
	{
		if (messageIsForward(msg)) {
			if (handleForward(c, chatId, it, msg, true)) {
				stop(it);
				return true;
			}
			return false;
		}
		easy_perform_sendMessage(c, chatId, "Ок, команда отменена.", TgMessageParse_Normal, 0);
		return true;
	}

	bool onUpdate(CURL *c, TgInteger fromId,
			TgInteger chatId, It it, const json &upd) override
	{
		if (upd.find("text") == upd.end()) {
			return true;
		}

		bool has_text = upd["text"].is_string();

		if (!has_text) {
			return true;
		}

		if (messageIsForward(upd)) {
			if (handleForward(c, chatId, it, upd, false)) {
				stop(it);
				return true;
			}
			return false;
		} else {
			if (handleMessage(c, chatId, it, upd)) {
				stop(it);
				return true;
			}
			return false;
		}
		return true;
	}
};

class AddAdminHandler : public ModifyAdminsHandler {

	void addUserToAdminsFromForward(CURL *c, TgInteger chatId, const json &msg)
	{
		auto &fwd = msg["forward_from"];
		TgInteger id = fwd["id"].get<TgInteger>();
		easy_perform_sendMessage(c, chatId,
				"Готово. Пользователь добавлен.", TgMessageParse_Normal, 0);
		h.addId(id);
		notifyListener(ModifyEntriesListener::ModifyAdminId);
	}

	virtual bool handleForward(CURL *c, TgInteger chatId, It currentUserId, const json &msg, bool fromCancel) override
	{
		addUserToAdminsFromForward(c, chatId, msg);
		return true;
	}

	bool handleMessage(CURL *c, TgInteger chatId, It currentUserId, const json &msg) override
	{
		std::string text = msg["text"].get<std::string>();
		if (messageIsAUsername(text)) {
			this->adminNamesList.push_back(text.substr(1));
			easy_perform_sendMessage(c, chatId, "Готово. Пользователь добавлен.", TgMessageParse_Normal, 0);
			notifyListener(ModifyEntriesListener::ModifyAdminName);
			return true;
		}
		return false;
	}
public:
	AddAdminHandler(const char *channelName, TgUsersList &ids,
			TgUserNamesList &names)
		: ModifyAdminsHandler(channelName, ids, names) {}

	bool onAddHandler(CURL *c, TgInteger fromId,
			TgInteger chatId) override
	{
		easy_perform_sendMessage(c, chatId, "Ок, отправь мне юзернейм (в виде @username), или форвард от пользователя, которого ты хочешь сделать редактором канала, или нажми /cancel для отмены.", TgMessageParse_Normal, 0);
		return true;
	}

};

class RemoveAdminHandler : public ModifyAdminsHandler {

	bool removeUserFromAdminListFromForward(CURL *c, TgInteger chatId, const json &msg)
	{
		auto &fwd = msg["forward_from"];
		TgInteger id = fwd["id"].get<TgInteger>();
		auto res = h.findId(id);
		if (h.haveResultId(res)) {
			easy_perform_sendMessage(c, chatId,
					"Готово. Пользователь удален.", TgMessageParse_Normal, 0);
			h.removeResultId(res);
			notifyListener(ModifyEntriesListener::ModifyAdminId);
			return true;
		} else {
			easy_perform_sendMessage(c, chatId, "Такой пользователь не найден. Команда отменена.", TgMessageParse_Normal, 0);
			return false;
		}
	}
protected:

	virtual bool handleForward(CURL *c, TgInteger chatId, It currentUserId, const json &msg, bool fromCancel) override
	{
		removeUserFromAdminListFromForward(c, chatId, msg);
		return true;
	}

	bool handleMessage(CURL *c, TgInteger chatId, It currentUserId, const json &msg) override
	{
		std::string text = msg["text"].get<std::string>();
		if (messageIsAUsername(text)) {
			auto res = std::find(this->adminNamesList.begin(),
					this->adminNamesList.end(), text.substr(1));
			if (res != this->adminNamesList.end()) {
				this->adminNamesList.erase(res);
				easy_perform_sendMessage(c, chatId, "Готово. Пользователь удален.", TgMessageParse_Normal, 0);
				notifyListener(ModifyEntriesListener::ModifyAdminName);
			} else {
				easy_perform_sendMessage(c, chatId, "Такой пользователь не найден. Команда отменена.", TgMessageParse_Normal, 0);
			}

			return true;
		}
		return false;
	}

public:
	RemoveAdminHandler(const char *name, TgUsersList &ids, TgUserNamesList &names)
	 : ModifyAdminsHandler(name, ids, names) {}

	bool onAddHandler(CURL *c, TgInteger fromId,
			TgInteger chatId) override
	{
		easy_perform_sendMessage(c, chatId, "Ок, отправь мне юзернейм (в виде @username), или форвард от пользователя, которого ты хочешь удалить из редакторов канала, или нажми /cancel для отмены.", TgMessageParse_Normal, 0);
		return true;
	}
};

class BotCommandsHandler : public UpdateHandler {
	std::map<std::string, std::unique_ptr<BotCommand> > commands;
public:
	bool handleUpdate(CURL *c, TgInteger fromId,
			TgInteger chatId, const json &upd) override
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
	TgUsersList &ids;
	TgUserNamesList &names;
public:
	PostCommandHandler(PhotoChannelPostHandler &ph,
			TgUsersList &ids, TgUserNamesList &names)
		: h(ph), ids(ids), names(names) {}

	virtual bool command(CURL *c, const json &msg, const std::string &cmd, size_t off, TgInteger fromId, TgInteger chatId) override
	{
		if (!ids.haveId(fromId)) {
			// check username.
			const auto &user = msg.find("from");
			if (user == msg.end() && !user->is_object()) {
				return false;
			}
			const auto &name = user->find("username");
			if (name == user->cend()) { 
				return false;
			}
			auto username = name->get<std::string>();
			if (username.empty()) {
				return false;
			}
			// username = username.substr(1);
			if (std::find(names.begin(), names.end(), username) == names.end()) {
				return false;
			}
		}

		h.addPostCommand(c, fromId, chatId); 

		return true;
	}
};

class AdminCommand : public BotCommand {
	PostHandler &h;
public:
	AdminCommand(PostHandler &hn) : h(hn) {}

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

// Storage classes.
template <typename DestStorage>
DestStorage &loadFromFile(const char *filename, DestStorage &dest)
{
	std::ifstream fs;
	fs.open(filename, std::ios_base::in);
	if (!fs.is_open()) {
		return dest;
	}
	typename DestStorage::value_type t;
	while (fs >> t) {
		dest.push_back(t);
	}
	return dest;
}

template <typename SourceStorage>
SourceStorage &saveToFile(const char *filename, SourceStorage &src)
{
	std::ofstream of;
	of.open(filename, std::ios_base::out);
	if (!of.is_open()) {
		return src;
	}

	for (auto &t : src) {
		of << t << std::endl;
	}
	return src;
}

class ListFlusher : public ModifyAdminsHandler::ModifyEntriesListener {
	TgUsersList &ids;
	TgUserNamesList &names;
	size_t timeout;
	size_t counter;
	bool updates[2];
public:
	ListFlusher(TgUsersList &ids, TgUserNamesList &names,
			size_t timeToFlush)
	: ids(ids), names(names), timeout(timeToFlush) {}

	void notify(NotifyType wh) override
	{
		updates[wh] = true;
		if (!counter) {
			counter = timeout;
		}
	}
	
	void countdown()
	{
		if (!counter) {
			return;
		}

		if (--counter == 0) {
			if (updates[ModifyAdminId]) {
				saveToFile(fileadminidslist, ids.vec());
				updates[ModifyAdminId] = false;
			}
			
			if (updates[ModifyAdminName]) {
				saveToFile(fileadminslist, names);
				updates[ModifyAdminName] = false;
			}
		}
	}

	void forceFlush()
	{
		saveToFile(fileadminslist, names);
		saveToFile(fileadminidslist, ids);
	}
};

// Bot global context
TgUsersList adminIdsList;
std::vector<std::string> adminNamesList;
PhotoChannelPostHandler photoPostHandler(idPostChannel, postChannelName);
BotCommandsHandler commandsHandler;
AddAdminHandler addAdminsHandler(postChannelName, adminIdsList, adminNamesList);
RemoveAdminHandler removeAdminsHandler(postChannelName, adminIdsList, adminNamesList);
ListFlusher flusher(adminIdsList, adminNamesList, 10);

// Update handlers.
class StorageCountdownUpdateHandler : public UpdateHandler {
public:
	bool handleUpdate(CURL *c, TgInteger fromId,
			TgInteger chatId, const json &msg) override
	{
		flusher.countdown();
		return false;
	}
};

class DismissNonMessagesUpdates : public UpdateHandler {

	json &update;
	json *&msg;
public:
	DismissNonMessagesUpdates(json &upd, json *&msg)
		: update(upd), msg(msg) {}

	bool handleUpdate(CURL *c, TgInteger fromId,
			TgInteger chatId, const json &msg) override
	{
		update.find("message");
		return true;
	}
};

class FromIdsFieldsIntilializer : public UpdateHandler {
	TgInteger &fromId;
	TgInteger &chatId;
public:
	FromIdsFieldsIntilializer(TgInteger &fromId, TgInteger &chatId)
		: fromId(fromId), chatId(chatId) {}

	bool handleUpdate(CURL *c, TgInteger fromId,
			TgInteger chatId, const json &msg) override
	{
		const auto &from = msg.find("from");
		
		if (from == msg.end()) {
			return true;
		}
		this->fromId = from.value()["id"].get<TgInteger>();

		auto &chat = msg["chat"];
		this->chatId = chat["id"].get<TgInteger>();
		return false; // allow to advance the msg to the next handler.
	}
};

class DismissNonTextMessagesHandler : public UpdateHandler {
	bool handleUpdate(CURL *c, TgInteger fromId,
			TgInteger chatId, const json &msg) override
	{
		return msg.find("text") == msg.end();
	}
};

void handle_update_message(CURL *c, json &res, bool &quit, size_t &updateOffset)
{
	static DismissNonTextMessagesHandler nonTextHandler;
	static StorageCountdownUpdateHandler storageCountdownHandler;
	TgInteger fromId = 0;
	TgInteger chatId = 0;
	FromIdsFieldsIntilializer fieldsInitializer(fromId, chatId);
	// json *msg = nullptr;
	TgInteger updId = res["update_id"].get<TgInteger>();

	if (res.find("message") == res.end()) {
		updateOffset = updId + 1;
		return;
	}

	auto &msg = res["message"];

	static UpdateHandler *handlers[] = {
		// nullptr,
		// nullptr,
		&fieldsInitializer,
		&storageCountdownHandler,
		&photoPostHandler,
		&addAdminsHandler,
		&removeAdminsHandler,
		// order matters.
		&nonTextHandler,
		&commandsHandler,
	};

	for (auto handler : handlers) {
		if (handler->handleUpdate(c, fromId, chatId, msg)) {
			break;
		}
	}
	
	updateOffset = updId + 1;
}

void handle_all_updates(CURL *c, json &upd, bool &quit, size_t &updateOffset)
{
	auto &r = upd["result"];
	if (!r.is_array()) {
		quit = true;
		return;
	}
	
	for (auto res : r) {
		handle_update_message(c, res, quit, updateOffset);
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

	auto sig = [](int sig) {
		flusher.forceFlush();
		exit(sig);
	};

	// Register signal handlers.
	signal(SIGINT, sig);
//	signal(SIGKILL, sig);
	signal(SIGTERM, sig);

	// Load from file.
	loadFromFile(fileadminslist, adminNamesList);
	loadFromFile(fileadminidslist, adminIdsList.vec());

	// Create bot commands.
	commandsHandler.addCommand(postCommandName,
			std::make_unique<PostCommandHandler>(photoPostHandler, adminIdsList, adminNamesList));
	commandsHandler.addCommand("start",
			std::make_unique<ResponseBotCommand>(
				std::string("Привет! Я @" BOT_NAME "! Я могу постить твои фотографии с подписями в канал. Чтобы это сделать, используй команду /") + postCommandName, TgMessageParse_Normal));

	// Admin commands.
	commandsHandler.addCommand("addadmin",
			std::make_unique<AdminCommand>(addAdminsHandler));
	commandsHandler.addCommand("adminlist",
			std::make_unique<PrintUserListCommand>(adminIdsList, adminNamesList));
	commandsHandler.addCommand("removeadmin",
			std::make_unique<AdminCommand>(removeAdminsHandler));

	// Add file flush listener
	addAdminsHandler.addListener(flusher);

	// The main message loop.
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
