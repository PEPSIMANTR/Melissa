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
	char* RequestPath; unsigned char RequestType;
	unsigned char CloseConnection;// There's no bool in C lol
	char* Host;
	unsigned long long RangeStart; unsigned long long RangeEnd;
	FILE* File;
};
struct ClientInfo ClientInfoDefault = {
	.CloseConnection = 0, .RequestPath = 0, .RequestType = 0, .RangeStart = 0, .RangeEnd = 0, .Host = 0, .File = 0
};

typedef struct HeaderParameters;
struct HeaderParameters {
	unsigned short StatusCode;
	size_t ContentLength; char* FileMime;
};
struct HeaderParameters HeaderParametersDefault = {
	.ContentLength = 0, .FileMime=0, .StatusCode=0
};









#endif