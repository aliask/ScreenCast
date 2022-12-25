#pragma once

#include "resource.h"

#define FRAME_IDENT 0x1234
#define COMMAND_IDENT 0x4321

#define IDT_SENDFRAME 1
#define IDT_SENDCOMMAND 2

// Milliseconds to stop the stream before sending a command
#define COMMAND_DELAY 300

typedef struct Frame {
	UINT16 ident;
	UINT16 height, width;
	UINT16 length;
	char data[];
} Frame;

typedef struct Command {
	UINT16 ident;
	UINT8 command;
	UINT8 value;
} Command;

enum Commands
{
	cmdBrightness = 0,
	cmdPriority = 1
};