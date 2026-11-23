#pragma once
#include <windows.h>
FARPROC WINAPI Pr7GetProcAddress(HMODULE,LPCSTR);
#define GetProcAddress Pr7GetProcAddress
