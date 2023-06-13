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
static inline void ClientInfoReset(struct ClientInfo* cl) {
	if (cl->RequestPath) free(cl->RequestPath);
	if (cl->Host) free(cl->Host);
}
static inline void ClientInfoCleanup(struct ClientInfo* cl) {
	ClientInfoReset(cl);
	free(cl->SendBuffer); free(cl->RecvBuffer);
}

typedef struct HeaderParameters;
struct HeaderParameters {
	unsigned short StatusCode;
	size_t ContentLength; char* FileMime;
};
struct HeaderParameters HeaderParametersDefault = {
	.ContentLength = 0, .FileMime=0, .StatusCode=0
};
static inline void HeaderParametersReset(struct HeaderParameters* h) {
	if (h->FileMime) free(h->FileMime);
}

typedef struct Vector;
struct Vector {
	unsigned char* Data;
	int CurrentSize; size_t MaxSize; unsigned int ElementSz;
};
struct Vector VectorDefault = {
	.Data = 0, .CurrentSize = 0, .MaxSize = 0, .ElementSz = 0
};
static inline void InitializeVector(struct Vector* Vec, int ElementSz) {
	Vec->ElementSz = ElementSz; Vec->Data = malloc(ElementSz + 1); Vec->Data[0] = 0; Vec->MaxSize = ElementSz + 1; return;
}
static inline void* AddElement(struct Vector* Vec, void* Element, int ElementSz) {
	if (ElementSz != Vec->ElementSz) abort(); //throw("AddElement(): Added element is not equal to vector element size.\n");
	// Search for an empty space first and reuse it
	size_t off = 0;
	for (; off < Vec->MaxSize; off+=ElementSz+1) {
		if (Vec->Data[off] != 0xFF) {// Assisgned spaces will have 0xFF byte at first byte
			Vec->Data[off] = 0xFF;
			memcpy(&Vec->Data[off + 1], Element, ElementSz);
			Vec->CurrentSize++; return Vec->Data[off + 1];
		}
	}
	// If not found, extend the vector and add to end.
	Vec->Data=realloc(Vec->Data, Vec->MaxSize + ElementSz + 1);
	if (!Vec->Data) abort();
	Vec->Data[Vec->MaxSize] = 0xFF;
	memcpy(&Vec->Data[Vec->MaxSize + 1], Element, ElementSz);
	Vec->MaxSize += ElementSz + 1; Vec->CurrentSize++; return Vec->MaxSize-ElementSz;
}
static inline void* GetElement(struct Vector* Vec, int Offset) {
	if (Offset > Vec->CurrentSize) abort(); //throw("GetElement(): Offset is bigger than current element count.\n");
	for (size_t i = 0; i < Vec->MaxSize; i+=Vec->ElementSz+1) {
		if (Vec->Data[i] == 0xFF) {
			if (!Offset) {
				return &Vec->Data[i + 1];
			}
			Offset--;
		}
	}
}
static inline void DeleteElement(struct Vector* Vec, int Offset) {
	if (Offset > Vec->CurrentSize) abort(); //throw("GetElement(): Offset is bigger than current element count.\n");
	for (size_t i = 0; i < Vec->MaxSize; i += Vec->ElementSz + 1) {
		if (Vec->Data[i] == 0xFF) {
			if (!Offset) {
				Vec->Data[i] = 0; Vec->CurrentSize--; return;
			}
			Offset--;
		}
	}
}






#endif