#include "Melissa.h"

void ParseHeader(struct ClientInfo cl) {
	int Received = recv(cl.s, cl.RecvBuffer, 4096, 0);
	send(cl.s, "HTTP/1.1 200 OK\r\n\Server: Melissa/0.0.1\r\nContent-Type: text/html\r\n\r\n<h1>dsadasdasd", 81, 0);
	shutdown(cl.s, 2); closesocket(cl.s); 
	free(cl.RecvBuffer); free(cl.SendBuffer);
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
	inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
	while (1) {
		SOCKET ClientSock = accept(listening, (struct sockaddr*)&client, &clientSize);
		if (ClientSock == SOCKET_ERROR) {
			printf("accept() failure.\n"); return -3;
		}
		struct ClientInfo cl; cl.s = ClientSock;
		cl.RecvBuffer = malloc(4096); cl.SendBuffer = malloc(4096);
		memset(cl.RecvBuffer, 0, 4095); memset(cl.SendBuffer, 0, 4096);
		ParseHeader(cl);
	}
	return 0;
}