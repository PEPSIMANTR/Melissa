#include "Melissa.h"
#pragma warning(disable:4996)

void ServerHeaders(struct HeaderParameters h, struct ClientInfo cl) {
	strcpy(cl.SendBuffer, "HTTP/1.1 "); int pos = 9;
	switch (h.StatusCode) {
		case 200: strcpy(&cl.SendBuffer[pos], "200 OK\r\n"); pos += 8; break;
		case 206: strcpy(&cl.SendBuffer[pos], "206 Partial Content\r\n"); pos += 21; break;
		case 400: strcpy(&cl.SendBuffer[pos], "400 Bad Request\r\n"); pos += 17; break;
		case 404: strcpy(&cl.SendBuffer[pos], "404 Not Found\r\n"); pos += 15; break;
		case 500: strcpy(&cl.SendBuffer[pos], "500 Internal Server Error\r\n"); pos += 27; break;
		case 501: strcpy(&cl.SendBuffer[pos], "501 Not Implemented\r\n"); pos += 21; break;
		default: strcpy(&cl.SendBuffer[pos], "501 Not Implemented\r\n"); pos += 21; break;
	}
	strcat(&cl.SendBuffer[pos], "Content-Length: "); pos += 16;
	if (h.ContentLength) {
		sprintf(&cl.SendBuffer[pos], "%lld", h.ContentLength);
		while (h.ContentLength) {
			pos++; h.ContentLength /= 10;
		}
	}
	else {
		cl.SendBuffer[pos] = '0'; pos++;
	}
	strcat(&cl.SendBuffer[pos], "\r\n"); pos += 2;
	if (h.FileMime) {
		strcat(&cl.SendBuffer[pos], "Content-Type: "); pos += 14;
		strcat(&cl.SendBuffer[pos], h.FileMime); pos += strlen(h.FileMime); strcat(&cl.SendBuffer[pos], "\r\n"); pos += 2;
	}
	strcat(&cl.SendBuffer[pos], "Server: Melissa/0.1\r\n"); pos += 21; strcat(&cl.SendBuffer[pos], "\r\n"); pos += 2;
	send(cl.s, cl.SendBuffer, pos, 0);
}

void Get(struct ClientInfo cl) {// Open the requested file, for now it'll handle whole request at once as polling is not implemented yet.
	struct HeaderParameters h=HeaderParametersDefault;
	cl.File = fopen(cl.RequestPath, "rb");
	if (cl.File) {// These will be replaced with system calls rather than stdio
		h.StatusCode = 200; size_t FileSize = 0;
		fseek(cl.File, 0, FILE_END); FileSize = ftell(cl.File); fseek(cl.File, 0, 0);
		h.ContentLength = FileSize; ServerHeaders(h, cl);
		while (FileSize) {
			if (FileSize > 4096) {
				fread(cl.SendBuffer, 4096, 1, cl.File); FileSize -= 4096;
				send(cl.s, cl.SendBuffer, 4096, 0);
			}
			else {
				fread(cl.SendBuffer, FileSize, 1, cl.File); 
				send(cl.s, cl.SendBuffer, FileSize, 0); FileSize = 0;
			}
		}
		fclose(cl.File);
	}
	else {
		h.StatusCode = 404; ServerHeaders(h, cl);
	}
	if (cl.CloseConnection) shutdown(cl.s, 2);
	return;
}

void ParseHeader(struct ClientInfo cl) {
	int Received = recv(cl.s, cl.RecvBuffer, 4096, 0);
	//send(cl.s, "HTTP/1.1 200 OK\r\n\Server: Melissa/0.0.1\r\nContent-Type: text/html\r\n\r\n<h1>dsadasdasd", 81, 0);
	//shutdown(cl.s, 2); closesocket(cl.s); 
	//free(cl.RecvBuffer); free(cl.SendBuffer);
	if (Received <= 0) {
		shutdown(cl.s, 2); closesocket(cl.s); free(cl.RecvBuffer); free(cl.SendBuffer);
	}
	unsigned short pos1 = 0, pos2 = 0;
	while (pos1 < Received) {
		if (cl.RecvBuffer[pos1] > 31) { pos1++; continue; }// Iterate till end of line

		if (cl.RequestPath == 0) {// We are on the first line.
			unsigned short end = pos1; pos1 = pos2;
			while (pos2 < end) {
				if (cl.RecvBuffer[pos2] != ' ') { pos2++; continue; }// Iterate till space
				if (cl.RequestType == 0) {// Request type
					if (!strncmp(&cl.RecvBuffer[pos1], "GET", 3)) cl.RequestType = 1;
					else cl.RequestType = 2;
					pos2++;
				}
				else if (cl.RequestPath == 0) {
					cl.RequestPath = malloc(pos2 + 2 - pos1); memset(cl.RequestPath, 0, pos2 + 2 - pos1);
					memcpy(&cl.RequestPath[1], &cl.RecvBuffer[pos1], pos2 - pos1); cl.RequestPath[0] = '.'; break;
				}
				pos1 = pos2;
			}
			pos2++;
			if (!strncmp(&cl.RecvBuffer[pos2], "HTTP/", 5)) {
				pos2 += 5;
				if (!strncmp(&cl.RecvBuffer[pos2], "1.0", 3))
					cl.CloseConnection = 1;
			}
		}
		// We are on later lines (key: value)
		else if(pos1-pos2>=6) {// Check if line is not shorter than header keys we're comparing for preventing an potential buffer overrun.
			if (!strncmp(&cl.RecvBuffer[pos2], "Host: ", 6)) {

			}
			else if (!strncmp(&cl.RecvBuffer[pos2], "Range: ", 7)) {

			}
		}
		else if (pos1 - pos2 == 0) {// We reached the empty line, which means end of headers.
			switch (cl.RequestType) {
				case 1:
					Get(cl);
				case 5:
				default:
					break;
			}
			return;
		}
		pos1++; if (&cl.RecvBuffer[pos1] < 32) pos1++;//Check if line delimiters are CRLF or LF
		pos2 = pos1;
	}
	return;
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
	struct Vector ClientVec = VectorDefault; InitializeVector(&ClientVec, sizeof(struct ClientInfo));
	struct pollfd EmptyPollfd = { .fd = DummySock,.events = 0 }; 
#ifdef _WIN32
	HANDLE VecMutex = CreateMutex(NULL, 0, NULL);
#endif
	char SockTypeElement = 0; struct pollfd PollfdElement = EmptyPollfd;
	SockTypeElement = 1; PollfdElement.fd = listening; PollfdElement.events = POLLIN;
	AddElement(&SockType, &SockTypeElement, 1); AddElement(&PollVec, &PollfdElement, sizeof(struct pollfd)); AddElement(&ClientVec, &ClientInfoDefault, sizeof(ClientInfoDefault));
	int Connections = 0;
	while ((Connections = WSAPoll(PollVec.Data, PollVec.MaxSize, -1)) > 0) {
		struct pollfd* PollPointer;
		for (size_t i = 0; i < PollVec.MaxSize; i++) {
			PollPointer = &PollVec.Data[i * sizeof(struct pollfd)];
			if (PollPointer->revents & POLLIN) {
				if (SockType.Data[i] & 1) {// Listening socket
					SOCKET client = accept(listening, NULL, NULL);
					PollfdElement.fd = client;
					AddElement(&PollVec, &PollfdElement, sizeof(PollfdElement));
					SockTypeElement = 0; AddElement(&SockType, &SockTypeElement, 1);
					struct ClientInfo cl = ClientInfoDefault; ClientInfoInit(&cl); cl.s = client;
					AddElement(&ClientVec, &cl, sizeof(cl));
				}
				else {// Client socket
					struct ClientInfo cl = ClientInfoDefault;
					memcpy(&cl, GetElement(&ClientVec, i), sizeof(cl));
					ParseHeader(cl);
				}
				Connections--;
			}
			else if (PollPointer->revents & POLLHUP) {// Connection terminated.
				ClientInfoCleanup(GetElement(&ClientVec, i)); memcpy(PollPointer, &EmptyPollfd, sizeof(EmptyPollfd));
			}
			if (!Connections) break;
		}
	}
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
	/*struct Vector v = VectorDefault;
	InitializeVector(&v, sizeof(int));
	int x = 0x7aaaaaaa;
	for (int i = 0;i < 5;i++) {
		AddElement(&v, &x, sizeof(int)); x++;
	}
	x = 0;
	for (size_t i = 0; i < 5; i++) {
		memcpy(&x, GetElement(&v, i), 4);
	}
	DeleteElement(&v, 2);
	AddElement(&v, &x, sizeof(int));*/

	return 0;
}