#include "Melissa.h"
#pragma warning(disable:4996)

void ServerHeaders(struct HeaderParameters* h, struct ClientInfo* cl) {
	memset(cl->SendBuffer, 0, 4096);
	strcpy(cl->SendBuffer, "HTTP/1.1 "); int pos = 9;
	switch (h->StatusCode) {
	case 200: strcpy(&cl->SendBuffer[pos], "200 OK\r\n"); pos += 8; break;
	case 206: strcpy(&cl->SendBuffer[pos], "206 Partial Content\r\n"); pos += 21; break;
	case 400: strcpy(&cl->SendBuffer[pos], "400 Bad Request\r\n"); pos += 17; break;
	case 404: strcpy(&cl->SendBuffer[pos], "404 Not Found\r\n"); pos += 15; break;
	case 500: strcpy(&cl->SendBuffer[pos], "500 Internal Server Error\r\n"); pos += 27; break;
	case 501: strcpy(&cl->SendBuffer[pos], "501 Not Implemented\r\n"); pos += 21; break;
	default: strcpy(&cl->SendBuffer[pos], "501 Not Implemented\r\n"); pos += 21; break;
	}
	strcat(&cl->SendBuffer[pos], "Content-Length: "); pos += 16;
	if (h->ContentLength) {
		sprintf(&cl->SendBuffer[pos], "%lld", h->ContentLength);
		while (h->ContentLength) {
			pos++; h->ContentLength /= 10;
		}
	}
	else {
		cl->SendBuffer[pos] = '0'; pos++;
	}
	strcat(&cl->SendBuffer[pos], "\r\n"); pos += 2;
	if (h->FileMime) {
		strcat(&cl->SendBuffer[pos], "Content-Type: "); pos += 14;
		strcat(&cl->SendBuffer[pos], h->FileMime); pos += strlen(h->FileMime); strcat(&cl->SendBuffer[pos], "\r\n"); pos += 2;
	}
	strcat(&cl->SendBuffer[pos], "Server: Melissa/0.1\r\n"); pos += 21; strcat(&cl->SendBuffer[pos], "\r\n"); pos += 2;
	if (send(cl->s, cl->SendBuffer, pos, 0) < 0)
		abort();
}

void Get(struct ClientRequest* cl) {// Open the requested file, for now it'll handle whole request at once as polling is not implemented yet.
	struct HeaderParameters h = HeaderParametersDefault;
	//cl->File = fopen(cl->RequestPath, "rb");
	if (cl->File) {// These will be replaced with system calls rather than stdio
		h.StatusCode = 200; size_t FileSize = 0;
		fseek(cl->File, 0, FILE_END); FileSize = ftell(cl->File); fseek(cl->File, 0, 0);
		h.ContentLength = FileSize; ServerHeaders(&h, cl);
		while (FileSize) {
			if (FileSize > 4096) {
				fread(cl->cl->SendBuffer, 4096, 1, cl->File); FileSize -= 4096;
				send(cl->cl->s, cl->cl->SendBuffer, 4096, 0);
			}
			else {
				fread(cl->cl->SendBuffer, FileSize, 1, cl->File);
				send(cl->cl->s, cl->cl->SendBuffer, FileSize, 0); FileSize = 0;
			}
		}
		fclose(cl->File);
	}
	else {
		h.StatusCode = 404; ServerHeaders(&h, cl->cl);
	}
	if (cl->CloseConnection) shutdown(cl->cl->s, 2);
	return;
}

void ParseHeader(struct ClientInfo* c) {
	struct ClientRequest cl = ClientRequestDefault; cl.cl = c;
	int Received = recv(c->s, c->RecvBuffer, 4096, 0);
	if (Received <= 0) {
		shutdown(c->s, 2); closesocket(c->s); return;
	}
	ClientRequestReset(&cl);
	unsigned short pos1 = 0, pos2 = 0;
	while (pos1 < Received) {
		if (c->RecvBuffer[pos1] > 31) { pos1++; continue; }// Iterate till end of line

		if (cl.RequestPath == 0) {// We are on the first line.
			unsigned short end = pos1; pos1 = pos2;
			while (pos2 < end) {
				if (c->RecvBuffer[pos2] != ' ') { pos2++; continue; }// Iterate till space
				if (cl.RequestType == 0) {// Request type
					if (!strncmp(&c->RecvBuffer[pos1], "GET", 3)) cl.RequestType = 1;
					else cl.RequestType = 2;
					pos2++;
				}
				else if (cl.RequestPath == 0) {
					cl.RequestPath = malloc(pos2 + 2 - pos1); memset(cl.RequestPath, 0, pos2 + 2 - pos1);
					memcpy(&cl.RequestPath[1], &c->RecvBuffer[pos1], pos2 - pos1); cl.RequestPath[0] = '.'; break;
				}
				pos1 = pos2;
			}
			pos2++;
			if (!strncmp(&c->RecvBuffer[pos2], "HTTP/", 5)) {
				pos2 += 5;
				if (!strncmp(&c->RecvBuffer[pos2], "1.0", 3))
					cl.CloseConnection = 1;
			}
		}
		// We are on later lines (key: value)
		else if (pos1 - pos2 >= 6) {// Check if line is not shorter than header keys we're comparing for preventing an potential buffer overrun.
			if (!strncmp(&c->RecvBuffer[pos2], "Host: ", 6)) {

			}
			else if (!strncmp(&c->RecvBuffer[pos2], "Range: ", 7)) {

			}
		}
		else if (pos1 - pos2 == 0) {// We reached the empty line, which means end of headers.
			switch (cl.RequestType) {
			case 1:
				Get(&cl);
			case 5:
			default:
				break;
			}
			return;
		}
		pos1++; if (&c->RecvBuffer[pos1] < 32) pos1++;//Check if line delimiters are CRLF or LF
		pos2 = pos1;
	}
	return;
}

void ThreadDriver(LPVOID lpParam) {
	//WinAPI is fucking broken. It somehow overwrites the parameter of this func, so i'll copy it.
	//Update: fopen on Get() makes it change. What the fuck Microsoft???
	struct ThreadInfo* t = lpParam;
	//memcpy(t, lpParam, sizeof(struct ThreadParameters));
	struct ClientInfo* ClientPtr = NULL;
	while (read(t->RecvPipe, &ClientPtr, sizeof(ClientPtr))) {
		//memset(t->OccupicationFlag, 0xFF, 1);
		ParseHeader(ClientPtr);
		printf("%d", t->ThreadID);
		//ClientPtr->PollPtr->events = POLLIN;
		//memset(t->OccupicationFlag, 0, 1);
	}
	switch (errno)
	{
	case EBADF:
		perror("Bad file descriptor!");
		break;
	case ENOSPC:
		perror("No space left on device!");
		break;
	case EINVAL:
		perror("Invalid parameter: buffer was NULL!");
		break;
	default:
		// An unrelated error occurred
		perror("Unexpected error!");
	}
	abort();
}

void ThreadDriver2(LPVOID lpParam) {
	//WinAPI is fucking broken. It somehow overwrites the parameter of this func, so i'll copy it.
	struct ThreadParameters* t = malloc(sizeof(struct ThreadParameters));
	memcpy(t, lpParam, sizeof(struct ThreadParameters));
	struct ClientInfo* ClientPtr = NULL;
	while (read(t->ti->RecvPipe, &ClientPtr, sizeof(ClientPtr))) {
		//memset(t->OccupicationFlag, 0xFF, 1);
		ParseHeader(ClientPtr);
		printf("%d", t->ti->ThreadID);
		//ClientPtr->PollPtr->events = POLLIN;
		//memset(t->OccupicationFlag, 0, 1);
	}
	abort();
}

int main() {
#ifdef _WIN32
	// Initialze winsock
	WSADATA wsData; WORD ver = MAKEWORD(2, 2);
	if (WSAStartup(ver, &wsData)) {
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
		//std::cout << "Error binding socket on port " << port[i] << std::endl << "Make sure port is not in use by another program."; 
		return -2;
	}
	//Linux can assign socket to different port than desired when is a small port number (or at leats that's what happening for me)
	else if (8000 != ntohs(hint.sin_port)) { /*std::cout << "Error binding socket on port " << port[i] << " (OS assigned socket on another port)" << std::endl << "Make sure port is not in use by another program, or you have permissions for listening that port." << std::endl;*/ return -2; }
	listen(listening, SOMAXCONN);

	// Listen the socket and accept the connection
	struct sockaddr_in client; char host[NI_MAXHOST] = { 0 }; // Client's IP address
#ifndef _WIN32
	unsigned int clientSize = sizeof(client);
#else
	int clientSize = sizeof(client);
#endif
	// Setup the structures
	inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
	struct Vector PollVec = VectorDefault; InitializeVector(&PollVec, sizeof(struct pollfd));
	struct Vector SockType = VectorDefault; InitializeVector(&SockType, 1);
	struct Vector ClientVec = VectorDefault; InitializeVector(&ClientVec, sizeof(struct ClientInfo*));
	struct Vector ThreadVec = VectorDefault; InitializeVector(&ThreadVec, sizeof(struct ThreadInfo*));
	//struct Vector ThreadPrVec = VectorDefault; InitializeVector(&ThreadPrVec, sizeof(struct ThreadParameters));
	//struct Vector ThreadHandleVec = VectorDefault; InitializeVector(&ThreadHandleVec, sizeof(HANDLE));
	struct Vector ThreadOccupancyVec = VectorDefault; InitializeVector(&ThreadOccupancyVec, 1);
	for (int i = 0; i < 1; i++) {
		struct ThreadInfo* t = malloc(sizeof(ThreadInfoDefault)); char zero = 0; int pipes[2] = { 0 }; struct ThreadParameters tp = ThreadParametersDefault;
		_pipe(&pipes, sizeof(void*), 0);
		t->RecvPipe = pipes[0]; t->SendPipe = pipes[1]; t->ThreadID = i + 1;
		//AddElement(&ThreadVec, &t, sizeof(t));
		AddElement(&ThreadOccupancyVec, &zero, 1);
		//tp.OccupicationFlag = &ThreadOccupancyVec.Data[i]; tp.ti = GetElement(&ThreadVec, i); AddElement(&ThreadPrVec, &tp, sizeof(tp));
		AddElement(&ThreadVec, &t, 8);
		if (!CreateThread(NULL, 0, ThreadDriver, t, 0, NULL)) abort();
	}
	struct pollfd EmptyPollfd = { .fd = DummySock,.events = 0 };
#ifdef _WIN32
	HANDLE VecMutex = CreateMutex(NULL, 0, NULL);
#endif
	char SockTypeElement = 0; struct pollfd PollfdElement = EmptyPollfd;
	SockTypeElement = 1; PollfdElement.fd = listening; PollfdElement.events = POLLIN;
	AddElement(&SockType, &SockTypeElement, 1); AddElement(&PollVec, &PollfdElement, sizeof(struct pollfd)); 
	AddElement(&ClientVec, &NullPtr, sizeof(NullPtr));
	int Connections = 0, EmptyElements = 0; struct pollfd* PollPointer;
	//goto xy;
	while ((Connections = WSAPoll(PollVec.Data, PollVec.MaxSize, -1)) > 0) {
		EmptyElements = 0;
		for (size_t i = 0; i < PollVec.MaxSize; i++) {
			if (PollVec.Occupied[i] == 0) { EmptyElements++; continue; }
			//PollPointer = &PollVec.Data[i * sizeof(struct pollfd)];
			PollPointer = GetElement(&PollVec, i - EmptyElements);
			if (PollPointer->revents & POLLIN) {
				if (SockType.Data[i] & 1) {// Listening socket
					SOCKET client = accept(listening, NULL, NULL);
					PollfdElement.fd = client;
					AddElement(&PollVec, &PollfdElement, sizeof(PollfdElement));
					SockTypeElement = 0; AddElement(&SockType, &SockTypeElement, 1);
					struct ClientInfo* cl = malloc(sizeof(struct ClientInfo)); ClientInfoInit(cl); cl->s = client; cl->PollPtr = PollPointer;
					AddElement(&ClientVec, &cl, sizeof(cl));
				}
				else {// Client socket
					/*struct ClientInfo cl = ClientInfoDefault;
					memcpy(&cl, GetElement(&ClientVec, i-EmptyElements), sizeof(cl));
					ParseHeader(cl);*/

					// Disable polling for that socket, subthreads will re-enable it when request is done.
					//PollPointer->events = 0;

					// Check for any free threads, pass the client to that thread.
					while (1) {
						for (char j = 0; j < 1; j++) {
							if (!ThreadOccupancyVec.Data[j]) {
								struct ThreadInfo* ThreadPtr = 0;
								memcpy(&ThreadPtr, &ThreadVec.Data[j], 8);
								if (_write(ThreadPtr->SendPipe, GetElement(&ClientVec, i - EmptyElements), sizeof(void*)) < 0)
									abort();

								goto PassedToThread;
							}
						}
						Sleep(1);
					}
				PassedToThread:;
				}
				Connections--;
			}
			else if (PollPointer->revents & POLLHUP) {// Connection terminated.
				struct ClientInfo* cl = NULL; memcpy(&cl, GetElement(&ClientVec, i - EmptyElements), 8);
				ClientInfoCleanup(cl); free(cl);
				SockType.Data[i] = 0;
				memcpy(PollPointer, &EmptyPollfd, sizeof(EmptyPollfd)); Connections--; DeleteElement(&PollVec, i - EmptyElements);
				DeleteElement(&ClientVec, i - EmptyElements); DeleteElement(&SockType, i - EmptyElements); EmptyElements++;
			}
			if (!Connections) break;
		}
	}
	wchar_t* s = NULL; int errnum = WSAGetLastError();
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errnum,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&s, 0, NULL);
	fprintf(stderr, "%S\n", s);
	abort();
	/*while (1) {
		SOCKET ClientSock = accept(listening, (struct sockaddr*)&client, &clientSize);
		if (ClientSock == SOCKET_ERROR) {
			printf("accept() failure.\n"); return -3;
		}
		struct ClientInfo cl = ClientInfoDefault; cl.s = ClientSock;
		cl.RecvBuffer = malloc(4096); cl.SendBuffer = malloc(4096);
		memset(cl.RecvBuffer, 0, 4095); memset(cl.SendBuffer, 0, 4096);
		ParseHeader(cl);
	}*/
	// Vector testing code
xy:
	struct Vector v = VectorDefault;
	InitializeVector(&v, sizeof(int));
	int x = 0x7aaaaaaa;
	for (int i = 0; i < 5; i++) {
		AddElement(&v, &x, sizeof(int)); x++;
	}
	x = 0;
	for (size_t i = 0; i < 5; i++) {
		memcpy(&x, GetElement(&v, i), 4);
	}
	DeleteElement(&v, 2);
	AddElement(&v, &x, sizeof(int));

	return 0;
}