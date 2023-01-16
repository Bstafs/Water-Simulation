#include "Debug.h"

Debug& Debug::GetInstance()
{
	static Debug instance;
	return instance;
}

void Debug::DebugWrite(char* string)
{
	OutputDebugStringA(string);
}

void Debug::DebugNum(static int num)
{
	char sz[1024] = { 0 };

	sprintf_s(sz, "%d \n", num);

	OutputDebugStringA(sz);
}

