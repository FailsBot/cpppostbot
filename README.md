[![Build Status](https://travis-ci.org/FailsBot/cpppostbot.svg?branch=master)](https://travis-ci.org/FailsBot/cpppostbot)
# Channel Post helper bot

## Configuration

* Fill in the `./cfg/channelid` with your channel _ID_ and the `./cfg/channelname` with your channel name for the bot, or else it can't help you with posting;
* Fill in the `./cfg/ownerid` and `./cfg/adminsids` your Telegram account ID (or else you can't use admin and post commands);
* Get a token and bot username from the @BotFather bot and fill in the file botkey.h;
* Build bot via `./build.sh` (or directly run the cmake in output directory with arguments);
* Run in shell / cron / .sh-script / etc.
* Have fun!

## Dependencies
* C++14 (you also could try to build with C++11 compiler);
* cmake;
* curl;
* nlohmann json (file json.hpp);

## Available Commands
* `/addadmin` - adds a new admin for your channel. The bot will ask you user ID or @username of new admin. This user can post messages to your channel using your _post_ command;
* `/cancel` - cancels current command;
* `/removeadmin` - removes admin from list. The bot will ask you user ID or @username of admin. Use the `/cancel` if you don't want to remove admin;
* _post_ command (default `/post`, see "Customizing Post Command" for change its name) - create a new post. This command is available only for users which user id is written in line of file `./cfg/adminsids` or @username is written in line of file `./cfg/adminsnames`. After using that command the bot will ask you for a message. So far it supports image messages, possible in future in will support any groups of messages.

## Customizing Post Command
You can change the _post_ command name writing the quoted string into the `./cfg/postcommandname` file.
