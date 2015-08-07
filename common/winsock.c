#include <stdio.h>
#include <errno.h>
#include "winsock.h"
#include "pdu.h"
#include "logging.h"

typedef struct
{
	int				id;
	const char*		message;
} error_entry_t, *error_entry_ptr;

error_entry_t errors[] =
{
	{ 0, "No error" },
	{ WSAEINTR, "Interrupted system call" },
	{ WSAEBADF, "Bad file number" },
	{ WSAEACCES, "Permission denied" },
	{ WSAEFAULT, "Bad address" },
	{ WSAEINVAL, "Invalid argument" },
	{ WSAEMFILE, "Too many open sockets" },
	{ WSAEWOULDBLOCK, "Operation would block" },
	{ WSAEINPROGRESS, "Operation now in progress" },
	{ WSAEALREADY, "Operation already in progress" },
	{ WSAENOTSOCK, "Socket operation on non-socket" },
	{ WSAEDESTADDRREQ, "Destination address required" },
	{ WSAEMSGSIZE, "Message too long" },
	{ WSAEPROTOTYPE, "Protocol wrong type for socket" },
	{ WSAENOPROTOOPT, "Bad protocol option" },
	{ WSAEPROTONOSUPPORT, "Protocol not supported" },
	{ WSAESOCKTNOSUPPORT, "Socket type not supported" },
	{ WSAEOPNOTSUPP, "Operation not supported on socket" },
	{ WSAEPFNOSUPPORT, "Protocol family not supported" },
	{ WSAEAFNOSUPPORT, "Address family not supported" },
	{ WSAEADDRINUSE, "Address already in use" },
	{ WSAEADDRNOTAVAIL, "Can't assign requested address" },
	{ WSAENETDOWN, "Network is down" },
	{ WSAENETUNREACH, "Network is unreachable" },
	{ WSAENETRESET, "Net connection reset" },
	{ WSAECONNABORTED, "Software caused connection abort" },
	{ WSAECONNRESET, "Connection reset by peer" },
	{ WSAENOBUFS, "No buffer space available" },
	{ WSAEISCONN, "Socket is already connected" },
	{ WSAENOTCONN, "Socket is not connected" },
	{ WSAESHUTDOWN, "Can't send after socket shutdown" },
	{ WSAETOOMANYREFS, "Too many references, can't splice" },
	{ WSAETIMEDOUT, "Connection timed out" },
	{ WSAECONNREFUSED, "Connection refused" },
	{ WSAELOOP, "Too many levels of symbolic links" },
	{ WSAENAMETOOLONG, "File name too long" },
	{ WSAEHOSTDOWN, "Host is down" },
	{ WSAEHOSTUNREACH, "No route to host" },
	{ WSAENOTEMPTY, "Directory not empty" },
	{ WSAEPROCLIM, "Too many processes" },
	{ WSAEUSERS, "Too many users" },
	{ WSAEDQUOT, "Disc quota exceeded" },
	{ WSAESTALE, "Stale NFS file handle" },
	{ WSAEREMOTE, "Too many levels of remote in path" },
	{ WSASYSNOTREADY, "Network system is unavailable" },
	{ WSAVERNOTSUPPORTED, "Winsock version out of range" },
	{ WSANOTINITIALISED, "WSAStartup not yet called" },
	{ WSAEDISCON, "Graceful shutdown in progress" },
	{ WSAHOST_NOT_FOUND, "Host not found" },
	{ WSANO_DATA, "No host data of that type was found" }
};

const int errors_len = sizeof(errors) / sizeof(error_entry_t);


// function similar in spirit to Unix's perror() that tacks a canned 
// interpretation of the value of WSAGetLastError() onto the end of a
// passed string, separated by a ": "
// generally, you should implement smarter error handling than this, 
// but for default cases and simple programs it is sufficient
//
// function returns a pointer to an internal static buffer, so you
// must copy the data from this function before you call it again.  It
// follows that this function is also not thread-safe.

const char* WSAGetLastErrorMessage(const char* message_prefix, int error_id /* = 0 */)
{
	error_entry_ptr end;
	error_entry_t target;
	int i;

	static char error_buffer[100];
	char target_id[50];

	if (EINVAL == strcpy_s(error_buffer, _countof(error_buffer), message_prefix))
		error_buffer[0] = '\0';
	strcat_s(error_buffer, _countof(error_buffer), ": ");

	end = &errors[errors_len];
	error_id = error_id ? error_id : WSAGetLastError();

	target.id = (error_id >= 0 ? error_id : -1);
	target.message = 0;

	if (target.id != -1) {
		for (i = 0; i < errors_len; i++)
			if (errors[i].id == target.id)
				strcat_s(error_buffer, _countof(error_buffer), errors[i].message);
	}
	else
		strcat_s(error_buffer, _countof(error_buffer), "unknown error");

	target_id[0] = '\0';
	sprintf_s(target_id, _countof(target_id), " (%d)", target.id);
	strcat_s(error_buffer, _countof(error_buffer), target_id);

	return error_buffer;
}


// gracefully shuts down the connection 
// returns true if we're successful, false otherwise

BOOL shutdown_connection(SOCKET socket)
{
	char read_buffer[BUFFER_KSIZE];

	log_debug_printf("...shuting down socket: %d", socket);
	
	if (shutdown(socket, SD_SEND) == SOCKET_ERROR) 
		return FALSE;
	
	// receive any extra data still sitting on the socket
	while (1) {
		int new_bytes = recv(socket, read_buffer, BUFFER_KSIZE, 0);
		if (new_bytes == SOCKET_ERROR) 
			break;
		else if (new_bytes != 0) 
			log_debug_printf("received %d unexpected bytes during shutdown", new_bytes);
		else 
			break;
	}

	if (closesocket(socket) == SOCKET_ERROR)
		return FALSE;

	log_debug_printf("socket: %d has been closed", socket);

	return TRUE;
}