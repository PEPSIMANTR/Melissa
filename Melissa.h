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
	unsigned char* Data; // Actual data
	unsigned char* Occupied; // Array for indicating which offsets are allocated with data or unused, 0xFF if used.
	int CurrentSize; int MaxSize; unsigned int ElementSz;
};
struct Vector VectorDefault = {
	.Data = 0, .Occupied=0, .CurrentSize = 0, .MaxSize = 0, .ElementSz = 0
};
static inline void InitializeVector(struct Vector* Vec, int ElementSz) {
	Vec->ElementSz = ElementSz; Vec->Data = malloc(ElementSz); 
	Vec->Occupied = malloc(1); Vec->Occupied[0] = 0;
	Vec->MaxSize = 1; return;
}
static inline void* AddElement(struct Vector* Vec, void* Element, int ElementSz) {
	if (ElementSz != Vec->ElementSz) abort(); //throw("AddElement(): Added element is not equal to vector element size.\n");
	// Search for an empty space first and reuse it
	size_t off = 0;
	for (; off < Vec->MaxSize; off++) {
		if (Vec->Occupied[off] != 0xFF) {// If this offset is unused(free)
			memcpy(&Vec->Data[off * Vec->ElementSz], Element, ElementSz);
			Vec->CurrentSize++;  Vec->Occupied[off] = 0xFF;
			return &Vec->Data[off * Vec->ElementSz];
		}
	}
	// If not found, extend the vector and add to end.
	Vec->Data = realloc(Vec->Data, (Vec->MaxSize + 1) * ElementSz);  Vec->Occupied = realloc(Vec->Occupied, Vec->MaxSize + 1);
	if (!Vec->Data || !Vec->Occupied) abort();
	memcpy(&Vec->Data[(Vec->MaxSize)*ElementSz], Element, ElementSz); Vec->Occupied[Vec->MaxSize] = 0xFF;
	Vec->MaxSize++; Vec->CurrentSize++; return &Vec->Data[Vec->MaxSize-ElementSz];
}
static inline void* GetElement(struct Vector* Vec, int Offset) {
	if (Offset > Vec->CurrentSize) abort(); //throw("GetElement(): Offset is bigger than current element count.\n");
	for (size_t i = 0; i < Vec->MaxSize; i++) {
		if (Vec->Occupied[i] == 0xFF) {
			if (!Offset) {
				return &Vec->Data[i*Vec->ElementSz];
			}
			Offset--;
		}
	}
}
static inline void DeleteElement(struct Vector* Vec, int Offset) {
	if (Offset > Vec->CurrentSize) abort(); //throw("GetElement(): Offset is bigger than current element count.\n");
	for (size_t i = 0; i < Vec->MaxSize; i++) {
		if (Vec->Occupied[i] == 0xFF) {
			if (!Offset) {
				Vec->Occupied[i] = 0; Vec->CurrentSize--; return;
			}
			Offset--;
		}
	}
}






#endif