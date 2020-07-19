#pragma once

#include "resource.h"

#define FRAME_IDENT 0x1234

typedef struct Frame {
	UINT16 ident;
	UINT16 height, width;
	UINT16 length;
	char data[];
} Frame;