#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "common.h"
#include "logging.h"
#include "messages.h"
#include "pdu.h"
#include "mem.h"
#include "config.h"


// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORTLEN 16
#define DEFAULT_MSGLEN 256

typedef struct
{
	ip_address_t connection_in;

	BOOL log_info;
	BOOL log_debug;
	BOOL log_error;
} config_t, *config_ptr;

config_t global_config = {
	.connection_in = { .ip = "localhost", .port = 27015 },
	.log_info = FALSE, .log_debug = TRUE, .log_error = FALSE };


const char LOG_PATH[] = "Logs\\";
const char LOG_FILENAME[] = "server.log";
char LOG_PATH_FILENAME[MAX_LOG_FILENAME * 2 + 1];
const char LOG_INDEX[] = "server.log.index";
char LOG_PATH_INDEX[MAX_LOG_FILENAME * 2 + 1];

IMPLEMENT_FUNCTION_LOG(global_config, log_info)
IMPLEMENT_FUNCTION_LOG(global_config, log_debug)
IMPLEMENT_FUNCTION_LOG(global_config, log_error)


DECODE_PDU(in, InputType);
ENCODE_PDU(out, OutputType);


int __cdecl main(void)
{
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	
	char sendbuf[DEFAULT_BUFLEN];
	int sendbuflen = DEFAULT_BUFLEN;
	
	// init log config
	sprintf_s(LOG_PATH_FILENAME, _countof(LOG_PATH_FILENAME), "%s%s", LOG_PATH, LOG_FILENAME);
	sprintf_s(LOG_PATH_INDEX, _countof(LOG_PATH_INDEX), "%s%s", LOG_PATH, LOG_INDEX);
	
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	char connection_in_port[DEFAULT_PORTLEN];
	sprintf_s(connection_in_port, DEFAULT_PORTLEN, "%d", global_config.connection_in.port);

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, connection_in_port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// Accept a client socket
	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// No longer need server socket
	closesocket(ListenSocket);
	
	asn1SccSint sendInvokeid = 0;
	
	InputType recvType;
	
	OutputType sendType;
	char sendMsg[] = "this is a test response";
	char sendResultMsg[] = "ok";
	strcpy_s(sendType.command.arr, DEFAULT_MSGLEN, sendMsg);
	sendType.command.nCount = strlen(sendMsg) + 1;
	strcpy_s(sendType.result.arr, DEFAULT_MSGLEN, sendResultMsg);
	sendType.result.nCount = strlen(sendResultMsg) + 1;

	// Receive until the peer shuts down the connection
	do {
		MEMZERO(recvbuf, recvbuflen);
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			queue_t pdus = { .head = NULL, .tail = NULL };
			queue_entry_ptr qe = NULL;
			pdu_ptr p = NULL;

			int recvbufptr = DEFAULT_BUFLEN;
			read_pdus_from_recv_buffer(recvbuf, recvbuflen, &recvbufptr, &pdus);

			qe = pdus.head;
			while (qe != NULL) {
				p = (pdu_ptr)(qe->ref);

				// Decode received message
				pdu_t recv_pdu;
				memcpy(recv_pdu.buffer, p->buffer, p->length);
				recv_pdu.length = p->length;
				int recvEncodeErr = decode_in_pdu(&recvType, &recv_pdu);
				if (recvEncodeErr)
					return 1;

				printf("Bytes received: %d (id: %d msg: %s) \n", iResult, (int)recvType.invokeid, recvType.command.arr);

				sendType.invokeid = recvType.invokeid;

				// Encode message to send
				pdu_ptr send_pdu = NULL;
				int sendEncodeErr = encode_out_pdu(&send_pdu, &sendType);
				if (sendEncodeErr)
					return 1;

				// Create a pdu to send
				int sendbufptr = DEFAULT_BUFLEN;
				write_pdu_to_send_buffer(send_pdu, sendbuf, sendbuflen, &sendbufptr);

				// Echo the buffer back to the sender
				iSendResult = send(ClientSocket, sendbuf, sendbufptr, 0);
				if (iSendResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					return 1;
				}
				
				printf("Bytes sent: %d\n", iSendResult);
				
				free_pdu(send_pdu);

				free_pdu(p);

				qe = qe->next;
			}
		}
		else if (iResult == 0)
			printf("Connection closing...\n");
		else  {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

	} while (iResult > 0);

	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}