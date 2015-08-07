#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "common.h"
#include "logging.h"
#include "messages.h"
#include "pdu.h"
#include "mem.h"
#include "config.h"


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORTLEN 16
#define DEFAULT_MSGLEN 256

typedef struct
{
	ip_address_t connection_out;

	BOOL log_info;
	BOOL log_debug;
	BOOL log_error;
} config_t, *config_ptr;

config_t global_config = {
	.connection_out = { .ip = "localhost", .port = 27014 },
	.log_info = FALSE, .log_debug = TRUE, .log_error = FALSE };


const char LOG_PATH[] = "Logs\\";
const char LOG_FILENAME[] = "client.log";
char LOG_PATH_FILENAME[MAX_LOG_FILENAME * 2 + 1];
const char LOG_INDEX[] = "client.log.index";
char LOG_PATH_INDEX[MAX_LOG_FILENAME * 2 + 1];

IMPLEMENT_FUNCTION_LOG(global_config, log_info)
IMPLEMENT_FUNCTION_LOG(global_config, log_debug)
IMPLEMENT_FUNCTION_LOG(global_config, log_error)


DECODE_PDU(in, OutputType);
ENCODE_PDU(out, InputType);


BOOL isActive = TRUE;

static void sigintHandler(int signum)
{
	isActive = FALSE;
}

int __cdecl main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	
	char sendbuf[DEFAULT_BUFLEN];
	int sendbuflen = DEFAULT_BUFLEN;
	
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	
	int iResult;
	
	// init log config
	sprintf_s(LOG_PATH_FILENAME, _countof(LOG_PATH_FILENAME), "%s%s", LOG_PATH, LOG_FILENAME);
	sprintf_s(LOG_PATH_INDEX, _countof(LOG_PATH_INDEX), "%s%s", LOG_PATH, LOG_INDEX);

	signal(SIGINT, sigintHandler);

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char connection_out_port[DEFAULT_PORTLEN];
	sprintf_s(connection_out_port, DEFAULT_PORTLEN, "%d", global_config.connection_out.port);

	// Resolve the server address and port
	iResult = getaddrinfo(global_config.connection_out.ip, connection_out_port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}


	asn1SccSint sendInvokeid = 0;
	
	char sendMsg[] = "this is a test request";
	InputType sendType;
	strcpy_s(sendType.command.arr, DEFAULT_MSGLEN, sendMsg);
	sendType.command.nCount = strlen(sendMsg) + 1;

	OutputType recvType;

	do {
		sendType.invokeid = ++sendInvokeid;

		// Encode message to send
		pdu_ptr send_pdu = NULL;
		int sendEncodeErr = encode_out_pdu(&send_pdu, &sendType);
		if (sendEncodeErr)
			return 1;

		// Create a pdu to send
		int sendbufptr = DEFAULT_BUFLEN;
		write_pdu_to_send_buffer(send_pdu, sendbuf, sendbuflen, &sendbufptr);

		// Send a pdu
		iResult = send(ConnectSocket, sendbuf, sendbufptr, 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return 1;
		}
		 
		printf("Bytes Sent: %d\n", iResult);
		
		free_pdu(send_pdu);

		// Receive a pdu
		MEMZERO(recvbuf, recvbuflen);
		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
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

				free_pdu(p);

				qe = qe->next;
			}

		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return 1;
		}
	
		Sleep(1000);

	} while (isActive);


	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}

	// Receive until the peer closes the connection
	do {

		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
			printf("Bytes received: %d\n", iResult);
		else if (iResult == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error: %d\n", WSAGetLastError());

	} while (iResult > 0);

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	return 0;
}