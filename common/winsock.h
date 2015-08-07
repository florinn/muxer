#ifndef __WINSOCK_H
#define __WINSOCK_H

#pragma comment(lib, "Ws2_32.lib")

#include <winsock2.h>
#include <Ws2tcpip.h>

const char* WSAGetLastErrorMessage(const char*, int);
BOOL shutdown_connection(SOCKET);

#endif // __WINSOCK_H