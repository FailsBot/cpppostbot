// Some Telegram-related types.
#ifndef TGBOTLIB_TGTYPES_H
#define TGBOTLIB_TGTYPES_H
#include <stdint.h>

typedef int64_t TgInteger; 

typedef struct TgLocation {
	float latitude;
	float longitude;
} TgLocation;

enum TgMessageParseMode {
	TgMessageParse_Normal,
	TgMessageParse_Markdown,
	TgMessageParse_Html,
};

#endif
