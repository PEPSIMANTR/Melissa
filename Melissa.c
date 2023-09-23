// Melissa HTTP Server Project
// 
// This is a HTTP Server that aims to be light and fast, up to a point that running as kernel extension is the target.
// It'll work as accelerator for a higher level HTTP server, mainly the Alyssa HTTP Server (https://github.com/PEPSIMANTR/AlyssaHTTPServer)
// Has (will have) some features as well such as range requests and more.
// 
// Copyright(C) 2024 Alyssa Software - AfferoGPLv3+ licensed.

#include "Melissa.h"
#pragma warning(disable:4996)
#define ThreadCnt 16
ThreadInfo* tiArray[ThreadCnt] = { NULL };
//struct pollfd tPipes[1] = { 0 }; // Thread send pipes array.
HANDLE tSmp[ThreadCnt] = { 0 }; // Thread semaphores array.
HANDLE ConsoleMtx;

void ParseHeader(ClientInfo* c, ThreadInfo* t);

void __cdecl ThreadMain(size_t id) {
	ThreadInfo* ti = tiArray[id]; ClientInfo* ci = NULL;
	while (1) {
		//if (read(ti->RecvPipe, &ci, sizeof(ci)) != sizeof(ci)) abort(); // Reading from thread pipe failed, abort.
		if (!ReadFile(ti->RecvPipe, &ci, 8, NULL, NULL)) abort();
		if (ci == NULL) abort();
		if (WaitForSingleObject(ci->Mtx, 0) == WAIT_OBJECT_0) {
			ParseHeader(ci, ti); ci->PollPtr->events = POLLIN;
#ifdef _DEBUG
			if (WaitForSingleObject(ConsoleMtx, INFINITE) != WAIT_OBJECT_0) abort();
			printf("%d OK\n", ci->PollPtr->fd);
			ReleaseMutex(ConsoleMtx);
		}
		else {
			if (WaitForSingleObject(ConsoleMtx, INFINITE) != WAIT_OBJECT_0) abort();
			printf("%d BUSY\n", ci->PollPtr->fd);
			ReleaseMutex(ConsoleMtx);
		}
#else
		}
#endif
		// else mutex could not be acquired.
		ReleaseSemaphore(tSmp[id], 1, NULL);
	}
}

void ServerHeaders(HeaderParameters* p, ThreadInfo* t) {
	unsigned short off = 9; strcpy(t->SendBuffer, "HTTP/1.1 ");
	switch (p->StatusCode) {
		case 200: strcat(t->SendBuffer, "200 OK\r\n"); off += 8; break;
		case 404: strcat(t->SendBuffer, "404 Not Found\r\n"); off += 15; break;
		default : strcat(t->SendBuffer, "501 Not Implemented\r\n"); off += 21; break;
	}
	off += sprintf(&t->SendBuffer[off], 
		"Content-Length: %lld\r\n"
		"Server: Melissa/0.2\r\n", p->ContentLength);
	if (p->ContentType)
		off += sprintf(&t->SendBuffer[off], "Content-Type: %s\r\n", p->ContentType);
	t->SendBuffer[off] = '\r'; t->SendBuffer[off + 1] = '\n'; off += 2;
	send(p->CliPtr->PollPtr->fd, t->SendBuffer, off, 0);
}

void Get(ClientInfo* c, ThreadInfo* t) {
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
		p.StatusCode = 200; ServerHeaders(&p, t);
		
		// Read the file.
		while (p.ContentLength >= 4096) { 
			fread(t->SendBuffer, 4096, 1, f); send(c->PollPtr->fd, t->SendBuffer, 4096, 0); 
			p.ContentLength -= 4096;
		}
		if (p.ContentLength) {
			fread(t->SendBuffer, p.ContentLength, 1, f); 
			send(c->PollPtr->fd, t->SendBuffer, p.ContentLength, 0);
		}
		fclose(f); return;
	}
	else {
		p.StatusCode = 404; ServerHeaders(&p, t); return;
	}
}

void ParseHeader(ClientInfo* c, ThreadInfo* t) {
	unsigned short Recv,
				   pos = 0;//Position of EOL
	if (c->LastRecvLine) {
		memcpy(t->RecvBuffer, c->LastRecvLine, sizeof(c->LastRecvLine));
		Recv = sizeof(c->LastRecvLine); free(c->LastRecvLine); c->LastRecvLine = NULL;
		Recv += recv(c->PollPtr->fd, &t->RecvBuffer[Recv], 4096 - Recv, 0);
	}
	else
		Recv = recv(c->PollPtr->fd, t->RecvBuffer, 4096, 0);
	if (c->Flags ^ 1) {// First line is not parsed yet.
		for (; pos < Recv; pos++)
			if (t->RecvBuffer[pos] < 32) {
				if (t->RecvBuffer[pos] > 0) {
					char _pos = 0;
					if (!strncmp(t->RecvBuffer, "GET", 3)) {
						c->ReqType = 1; _pos = 4;
					}
					else if (!strncmp(t->RecvBuffer, "POST", 4)) {
						c->ReqType = 2; _pos = 5;
					}
					else if (!strncmp(t->RecvBuffer, "HEAD", 4)) {
						c->ReqType = 5; _pos = 5;
					}
					else {
						
					}
					c->ReqPath = malloc(pos - _pos - 1); memcpy(c->ReqPath, "htroot/", 7); memcpy(&c->ReqPath[7], &t->RecvBuffer[_pos], pos - _pos - 9); c->ReqPath[pos - _pos - 2] = 0;
					if (!strncmp(&t->RecvBuffer[pos - 8], "HTTP/1.", 7)) { 
						c->Flags |= 1; 
						if (t->RecvBuffer[pos - 1] == '0') {// HTTP/1.0 client
							c->Close = 1;
						}
						pos++;
						if (t->RecvBuffer[pos] < 31) pos++; // line delimiters are CRLF, iterate pos one more.
						break;
					}
					else {

					}
				}
				else {
					c->LastRecvLine = malloc(Recv);
					memcpy(c->LastRecvLine, t->RecvBuffer, Recv); 
					goto ParseReturn;
				}
			}
	}
	// Parse the lines
	for (unsigned short i = pos; i < Recv; i++) {
		if (t->RecvBuffer[i] > 31) continue;
		if (pos - i == 1) {// End of headers
			switch (c->ReqType) {
				case 1:
					Get(c, t);
				default:
					break;
			}
			if (c->Close) shutdown(c->PollPtr->fd, 2);
			ciReset(c); goto ParseReturn;
		}
		else if (!strncmp(&t->RecvBuffer[pos], "Connection", 10)) {
			if (!strcmp(&t->RecvBuffer[pos + 12], "close")) c->Close = 1;
			else c->Close = 0;
		}
		else if (!strncmp(&t->RecvBuffer[pos], "Host", 4)) {// Headers will be parsed that way, you got the point. + offsets also includes the ": ".
			//memcpy(&t->RecvBuffer[pos + 6, c->Host, pos - pos - 6]);
#ifdef _DEBUG
			//printf("asdasd");
#endif
		}
		pos = i+1;
		if (t->RecvBuffer[pos] < 31) pos++; // line delimiters are CRLF, iterate pos one more.
	}
	// All complete lines are parsed, check if there's a incomplete remainder
	if (pos < Recv) {
		c->LastRecvLine = malloc(Recv - pos); memcpy(c->LastRecvLine, t->RecvBuffer[pos], Recv - pos);
	}
ParseReturn:
	memset(t->RecvBuffer, 0, Recv); ReleaseMutex(c->Mtx); return;
}

int main() {
	ConsoleMtx = CreateMutex(NULL, 0, NULL);
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

	// Set up polling
	struct pollfd PollArray[256]; unsigned short PollCnt = 1;
	ClientInfo* ciArray[256] = { NULL };
	PollArray[0] = (struct pollfd){ .fd = listening, .events = POLLIN };
	unsigned short Connections; ciArray[0] = 31;

	// Init threads
	for (char i = 0; i < ThreadCnt; i++) {
		tiArray[i] = malloc(sizeof(ThreadInfo));
		tiArray[i]->ThreadID = i; //_pipe(&tiArray[i]->Pipes, 8, 0);
		if (!CreatePipe(&tiArray[i]->RecvPipe, &tiArray[i]->SendPipe, NULL, 8)) abort();
		tiArray[i]->SendBuffer = malloc(4096); memset(tiArray[i]->SendBuffer, 0, 4096);
		tiArray[i]->RecvBuffer = malloc(4096); memset(tiArray[i]->RecvBuffer, 0, 4096);
		//tPipes[i].fd = tiArray[i]->RecvPipe; tPipes[i].events = POLLOUT; tPipes[i].revents = 0;
		tSmp[i] = CreateSemaphore(NULL, 1, 1, NULL);
		tiArray[i]->Handle = CreateThread(NULL, 0, ThreadMain, i, 0, NULL);
	}

	// Start polling
	while ((Connections=WSAPoll(PollArray, PollCnt, -1))>0) {
		if (PollArray[0].revents & POLLIN) {// Incoming connection, accept.
			Connections--; SOCKET ClientSock = accept(listening, (struct sockaddr*)&client.sin_addr, &clientSize);
			// Search for freed space in array
			for (unsigned short i = 1; i < PollCnt; i++) {
				if (PollArray[i].fd == DummySock) {
					PollArray[i] = (struct pollfd){
						.fd = ClientSock, .events = POLLIN
					};
					ciArray[i] = malloc(sizeof(ClientInfo)); ciInit(ciArray[i]);
					ciArray[i]->PollPtr = &PollArray[i]; ciArray[i]->Mtx = CreateMutex(NULL, FALSE, NULL);
					goto IncomingAdded;
				}
			}
			// If none, emplace back
			PollArray[PollCnt] = (struct pollfd){
				.fd = ClientSock, .events = POLLIN
			}; 
			ciArray[PollCnt] = malloc(sizeof(ClientInfo)); ciInit(ciArray[PollCnt]);
			ciArray[PollCnt]->PollPtr = &PollArray[PollCnt]; ciArray[PollCnt]->Mtx = CreateMutex(NULL, FALSE, NULL);
			PollCnt++;
		}
	IncomingAdded:
		for (unsigned short i = 1; i < 255; i++) {
			if (PollArray[i].revents & POLLIN) {// Incoming data from client
				int pollret = 0;
				/*if ((pollret = poll(tPipes, ThreadCnt, 0)) > 0) {
					for (char x = 0; i < ThreadCnt; i++) {
						if (tPipes[x].revents & POLLOUT) {
							write(tPipes[x].fd, &ciArray[i], sizeof(ClientInfo*)); goto AddedToThread;
						}
					}
				}
				else {
					pollret = WSAGetLastError();
					abort();
				}*/
				pollret = WaitForMultipleObjects(ThreadCnt, tSmp, FALSE, 0);
#ifdef _DEBUG
				WaitForSingleObject(ConsoleMtx, INFINITE);
				printf("%d: %d\n", PollArray[i].fd, pollret);
				ReleaseMutex(ConsoleMtx);
#endif
				if (pollret == WAIT_TIMEOUT) goto AddedToThread;
				else if (pollret >= WAIT_ABANDONED) {
					pollret = GetLastError();
					abort();
				}
				else if (pollret >= WAIT_OBJECT_0) {
					PollArray[i].events = 0;
					//write(tPipes[pollret], &ciArray[i], sizeof(ClientInfo*));
					if (!WriteFile(tiArray[pollret]->SendPipe, &ciArray[i], sizeof(ClientInfo*), NULL, NULL)) abort();
				}
				AddedToThread:
				Connections--;
			}
			else if (PollArray[i].revents & (POLLERR | POLLHUP)) { // Connection lost to client.
				if (WaitForSingleObject(ciArray[i]->Mtx, 0) == WAIT_OBJECT_0) {// Mutex acquired.
					closesocket(PollArray[i].fd); 

					if (WaitForSingleObject(ConsoleMtx, INFINITE) != WAIT_OBJECT_0) abort();
					printf("%d GONE\n", PollArray[i].fd);
					ReleaseMutex(ConsoleMtx);

					CloseHandle(ciArray[i]->Mtx);
					free(ciArray[i]); ciArray[i] = NULL;
					PollArray[i] = EmptyPollfd;
				}
				Connections--;
			}
			if (!Connections) break;
		}
	}
}