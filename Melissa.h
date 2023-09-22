#pragma once
// Inclusions
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
	#include <WS2tcpip.h>
	#pragma comment(lib,"WS2_32.lib")
	#include <io.h>
#else
	
#endif
// Cross platform

// Structures
typedef struct {
	struct pollfd* PollPtr;
	char* ReqPath;
	unsigned char ReqType;
	char* LastRecvLine; // Last received incomplete line piece. 
	char Flags;
	char Close;
#ifdef _WIN32
	HANDLE Mtx;
#endif
} ClientInfo;
ClientInfo ciDefault = {
	.Close = 0, .Flags = 0, .ReqType = 0,
};
#define ciReset(name) name->Close=0; name->Flags=0; name->ReqType=0; free(name->ReqPath); name->ReqPath=NULL
#define ciInit(name) name->ReqPath=NULL; ciReset(name); name->LastRecvLine=NULL

typedef struct {
	size_t ContentLength;
	unsigned short StatusCode;
	char* ContentType;
	ClientInfo* CliPtr;
} HeaderParameters ;
HeaderParameters hpDefault = {
	.ContentLength = 0, .StatusCode = 404, .ContentType = 0, .CliPtr = 0
};
// Variables & constants

// Functions