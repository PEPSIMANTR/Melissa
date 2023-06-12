#pragma once
#ifndef MelissaHeader
#define MelissaHeader
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
	#include <WS2tcpip.h>
	#pragma comment (lib, "ws2_32.lib")
	#include <io.h>
#endif // _WIN32

typedef struct ClientInfo;
struct ClientInfo {
	SOCKET s;
	char* SendBuffer; char* RecvBuffer;
	
};








#endif