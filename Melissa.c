// Melissa HTTP Server Project
// 
// This is a HTTP Server that aims to be light and fast, up to a point that running as kernel extension is the target.
// It'll work as accelerator for a higher level HTTP server, mainly the Alyssa HTTP Server (https://github.com/PEPSIMANTR/AlyssaHTTPServer)
// Has (will have) some features as well such as range requests and more.
// 
// Copyright(C) 2024 Alyssa Software - AfferoGPLv3+ licensed.

#include "Melissa.h"
#pragma warning(disable:4996)

char* SendBuffer; char* RecvBuffer;

void ServerHeaders(HeaderParameters* p) {
	unsigned short off = 9; strcpy(SendBuffer, "HTTP/1.1 ");
	switch (p->StatusCode) {
		case 200: strcat(SendBuffer, "200 OK\r\n"); off += 8; break;
		case 404: strcat(SendBuffer, "404 Not Found\r\n"); off += 15; break;
		default : strcat(SendBuffer, "501 Not Implemented\r\n"); off += 21; break;
	}
	off += sprintf(&SendBuffer[off], 
		"Content-Length: %lld\r\n"
		"Server: Melissa/0.1\r\n", p->ContentLength);
	if (p->ContentType)
		off += sprintf(&SendBuffer[off], "Content-Type: %s\r\n", p->ContentType);
	SendBuffer[off] = '\r'; SendBuffer[off + 1] = '\n'; off += 2;
	send(p->CliPtr->PollPtr->fd, SendBuffer, off, 0);
}

void Get(ClientInfo* c) {
	HeaderParameters p = hpDefault; p.CliPtr = c;
	FILE* f = fopen(c->ReqPath, "rb");
	if (f) {// File opened successfully.
//#ifdef _WIN32
//		LARGE_INTEGER _fs; int x = GetFileSizeEx(f, &_fs);
//		if (!x) { x = GetLastError(); }
//		p.ContentLength = _fs.QuadPart;
//#endif
		fseek(f, 0, SEEK_END); p.ContentLength = _ftelli64(f); fseek(f, 0, SEEK_SET);
		p.ContentType = "text/html";
		p.StatusCode = 200; ServerHeaders(&p);
		
		// Read the file.
		while (p.ContentLength >= 4096) { 
			fread(SendBuffer, 4096, 1, f); send(c->PollPtr->fd, SendBuffer, 4096, 0); 
			p.ContentLength -= 4096;
		}
		if (p.ContentLength) {
			fread(SendBuffer, p.ContentLength, 1, f); send(c->PollPtr->fd, SendBuffer, p.ContentLength, 0);
		}
		fclose(f); return;
	}
	else {
		p.StatusCode = 404; ServerHeaders(&p); return;
	}
}

void ParseHeader(ClientInfo* c) {
	if (WaitForSingleObject(c->Mtx, 100) != WAIT_OBJECT_0) return; // Cannot acquire mutex, return.
	unsigned short Recv,
				   pos = 0;//Position of EOL
	if (c->LastRecvLine) {
		memcpy(RecvBuffer, c->LastRecvLine, sizeof(c->LastRecvLine));
		Recv = sizeof(c->LastRecvLine); free(c->LastRecvLine); c->LastRecvLine = NULL;
		Recv += recv(c->PollPtr->fd, &RecvBuffer[Recv], 4096 - Recv, 0);
	}
	else
		Recv = recv(c->PollPtr->fd, RecvBuffer, 4096, 0);
	if (c->Flags ^ 1) {// First line is not parsed yet.
		for (; pos < Recv; pos++)
			if (RecvBuffer[pos] < 32) {
				if (RecvBuffer[pos] > 0) {
					char _pos = 0;
					if (!strncmp(RecvBuffer, "GET", 3)) {
						c->ReqType = 1; _pos = 4;
					}
					else if (!strncmp(RecvBuffer, "POST", 4)) {
						c->ReqType = 2; _pos = 5;
					}
					else if (!strncmp(RecvBuffer, "HEAD", 4)) {
						c->ReqType = 5; _pos = 5;
					}
					else {
						
					}
					c->ReqPath = malloc(pos - _pos - 1); memcpy(c->ReqPath, "htroot/", 7); memcpy(&c->ReqPath[7], &RecvBuffer[_pos], pos - _pos - 9); c->ReqPath[pos - _pos - 2] = 0;
					if (!strncmp(&RecvBuffer[pos - 8], "HTTP/1.", 7)) { 
						c->Flags |= 1; 
						if (RecvBuffer[pos - 1] == '0') {// HTTP/1.0 client
							c->Close = 1;
						}
						pos++;
						if (RecvBuffer[pos] < 31) pos++; // line delimiters are CRLF, iterate pos one more.
						break;
					}
					else {

					}
				}
				else {
					c->LastRecvLine = malloc(Recv);
					memcpy(c->LastRecvLine, RecvBuffer, Recv); 
					goto ParseReturn;
				}
			}
	}
	// Parse the lines
	for (unsigned short i = pos; i < Recv; i++) {
		if (RecvBuffer[i] > 31) continue;
		if (pos - i == 1) {// End of headers
			switch (c->ReqType) {
				case 1:
					Get(c);
				default:
					break;
			}
			if (c->Close) shutdown(c->PollPtr->fd, 2);
			ciReset(c); goto ParseReturn;
		}
		else if (!strncmp(&RecvBuffer[pos], "Connection", 10)) {
			if (!strcmp(&RecvBuffer[pos + 12], "close")) c->Close = 1;
			else c->Close = 0;
		}
		else if (!strncmp(&RecvBuffer[pos], "Host", 4)) {// Headers will be parsed that way, you got the point. + offsets also includes the ": ".
			//memcpy(&RecvBuffer[pos + 6, c->Host, pos - pos - 6]);
#ifdef _DEBUG
			printf("asdasd");
#endif
		}
		pos = i+1;
		if (RecvBuffer[pos] < 31) pos++; // line delimiters are CRLF, iterate pos one more.
	}
	// All complete lines are parsed, check if there's a incomplete remainder
	if (pos < Recv) {
		c->LastRecvLine = malloc(Recv - pos); memcpy(c->LastRecvLine, RecvBuffer[pos], Recv - pos);
	}
ParseReturn:
	memset(RecvBuffer, 0, Recv); ReleaseMutex(c->Mtx); return;
}

int main() {
	SendBuffer = malloc(4096); RecvBuffer = malloc(4096);
	memset(SendBuffer, 0, 4096); memset(RecvBuffer, 0, 4096);
#ifdef _WIN32
	// Initialize WinSock
	WSADATA _WSADATA; WORD _wsver = MAKEWORD(2, 2);
	if (WSAStartup(_wsver, &_WSADATA)) {
		printf("Can't Initialize winsock! Quitting\n"); return -1;
	}
#endif
	// Create a socket
	SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
	if (listening == INVALID_SOCKET) {
		printf("Can't create a socket! Quitting\n"); return -1;
	}
	SOCKET DummySock = socket(AF_INET, SOCK_STREAM, 0);
	if (DummySock == INVALID_SOCKET) {
		printf("Can't create a socket! Quitting\n"); return -1;
	}

	struct sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(8000);
	inet_pton(AF_INET, "0.0.0.0", &hint.sin_addr);
	socklen_t len = sizeof(hint);
	bind(listening, (struct sockaddr*)&hint, sizeof(hint));
	if (getsockname(listening, (struct sockaddr*)&hint, &len) == -1) {//Cannot reserve socket
		printf("Error binding socket on port %d\n Make sure port is not in use by another program.\n", 8000);
		return -2;
	}
	//Linux can assign socket to different port than desired when is a small port number (or at leats that's what happening for me)
	else if (8000 != ntohs(hint.sin_port)) { 
		printf("Error binding socket on port %d (OS assigned socket on another port)\n Make sure port is not in use by another program, or you have permissions for listening that port.\n", 8000);
		return -2; 
	}
	listen(listening, SOMAXCONN);

	// Client stuff
	struct sockaddr_in client; char host[NI_MAXHOST] = { 0 }; // Client's IP address
#ifndef _WIN32
	unsigned int clientSize = sizeof(client);
#else
	int clientSize = sizeof(client);
#endif
	// Setup the structures
	inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);

	struct pollfd EmptyPollfd = { .fd = DummySock,.events = 0 };
	char SockTypeElement = 0; struct pollfd PollfdElement = EmptyPollfd;
	SockTypeElement = 1; PollfdElement.fd = listening; PollfdElement.events = POLLIN;

	// Start polling
	struct pollfd PollArray[256]; unsigned short PollCnt = 1;
	ClientInfo* ciArray[256] = { NULL };
	PollArray[0] = (struct pollfd){ .fd = listening, .events = POLLIN };
	unsigned short Connections;

	while ((Connections=WSAPoll(PollArray, PollCnt, -1))>0) {
		if (PollArray[0].revents & POLLIN) {// Incoming connection, accept.
			Connections--; SOCKET ClientSock = accept(listening, (struct sockaddr*)&client.sin_addr, &clientSize);
			// Search for freed space in array
			for (unsigned short i = 1; i < PollCnt; i++) {
				if (PollArray[i].fd = DummySock) {
					PollArray[i] = (struct pollfd){
						.fd = ClientSock, .events = POLLIN
					};
					ciArray[i] = malloc(sizeof(ClientInfo)); ciInit(ciArray[i]);
					ciArray[i]->PollPtr = &PollArray[i]; ciArray[i]->Mtx = CreateMutex(NULL, FALSE, NULL);
					ParseHeader(ciArray[i]); goto IncomingAdded;
				}
			}
			// If none, emplace back
			PollArray[PollCnt] = (struct pollfd){
				.fd = ClientSock, .events = POLLIN
			}; 
			ciArray[PollCnt] = malloc(sizeof(ClientInfo)); ciInit(ciArray[PollCnt]);
			ciArray[PollCnt]->PollPtr = &PollArray[PollCnt]; ciArray[PollCnt]->Mtx = CreateMutex(NULL, FALSE, NULL);
			// Handle that client too
			ParseHeader(ciArray[PollCnt]);
			PollCnt++;
		}
	IncomingAdded:
		for (unsigned short i = 1; Connections; i++) {
			if (PollArray[i].revents & POLLIN) {// Incoming data from client
				ParseHeader(ciArray[i]);
				Connections--;
			}
			else if (PollArray[i].revents & (POLLERR | POLLHUP)) { // Connection lost to client.
				if (WaitForSingleObject(ciArray[i]->Mtx, 0) == WAIT_OBJECT_0) {// Mutex acquired.
					closesocket(PollArray[i].fd); PollArray[i] = EmptyPollfd; 
					ReleaseMutex(ciArray[i]->Mtx); CloseHandle(ciArray[i]->Mtx);
					free(ciArray[i]); ciArray[i] = NULL;
				}
				Connections--;
			}
		}
	}
}