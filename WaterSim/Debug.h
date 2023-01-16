#pragma once

#include <string>
#include <Windows.h>

class Debug
{
public:
	static Debug& GetInstance();
	void DebugWrite(char* string);
	void DebugNum(static int num);
	void DebugCursor(static int x, static int y);

};

