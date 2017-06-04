# Channel Post helper bot

## Configuration

* Fill in the ./cfg/channelid and the ./cfg/channelname for the bot, or else it can't help you with posting;
* Get a token from the @BotFather bot and fill in the file botkey.h;
* Build bot via ./build.sh (or directly run the cmake in output directory with arguments;
* Run in shell / cron / .sh-script / etc.
* Have fun!

## Dependencies
* C++11;
* cmake;
* curl;
* nlohmann json (file json.hpp);
* to\_string workaround for Android NDK. You may disable it while configuring the cmake build.
