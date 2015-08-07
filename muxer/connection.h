#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "winsock.h"
#include "queue.h"
#include "global.h"

typedef struct
{
	SOCKET			socket;

	ip_address_t	ip_address;

	unsigned char	r_buffer[BUFFER_KSIZE];
	size_t			r_buffer_ptr;
	unsigned char	w_buffer[BUFFER_KSIZE];
	size_t			w_buffer_ptr;
} connection_t, *connection_ptr;

extern queue_t				connections;

extern CRITICAL_SECTION		csec_connections;

connection_ptr alloc_connection(SOCKET socket, char* ip_addr, unsigned int ip_port);
void remove_connection(SOCKET socket);
void free_connection(connection_ptr p);
void remove_all_connections(void);

#endif // __CONNECTION_H