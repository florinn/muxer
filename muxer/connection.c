#include "connection.h"
#include "global.h"
#include "mapping.h"

queue_t	connections;

CRITICAL_SECTION csec_connections;

connection_ptr alloc_connection(SOCKET socket, char* ip_addr, unsigned int ip_port)
{
	connection_ptr new_connection = NULL;

	assert(socket > 0);
	assert(ip_addr != NULL);

	new_connection = memalloc(sizeof(connection_t));
	if (new_connection == NULL)
		return NULL;

	new_connection->socket = socket;
	strcpy_s(new_connection->ip_address.ip, _countof(new_connection->ip_address.ip), ip_addr);
	new_connection->ip_address.port = ip_port;

	MEMZERO(new_connection->r_buffer, _countof(new_connection->r_buffer));
	new_connection->r_buffer_ptr = 0;
	MEMZERO(new_connection->w_buffer, _countof(new_connection->w_buffer));
	new_connection->w_buffer_ptr = 0;

	return new_connection;
}

void free_connection(connection_ptr p)
{
	if (p != NULL) {
		free(p);
		p = NULL;
	}
}

void remove_connection(SOCKET socket)
{
	queue_entry_ptr qe = NULL, qe_prev = NULL;
	connection_ptr p = NULL;
	unsigned int count = 0;

	qe = connections.head;

	while (qe != NULL) {
		p = (connection_ptr)(qe->ref);
		assert(p != NULL);

		if (p->socket == socket) {
			
			remove_all_mappings_for(p->socket);
			
			qe->ref = NULL;
			qe_prev = qe;
			qe = qe->next;

			q_free(&connections, qe_prev);
			free_connection(p);
			
			count++;
			continue;
		}

		qe = qe->next;
	}

	assert(count <= 1);

	log_debug_printf("done removing %d connection(s) associated to closed Socket: %d", count, socket);
}

void remove_all_connections(void)
{
	queue_entry_ptr qe_connections = NULL;
	connection_ptr r = NULL;
	int count = 0;

	EnterCriticalSection(&csec_connections);

	qe_connections = connections.head;

	while (qe_connections != NULL) {
		r = (connection_ptr)qe_connections->ref;

		assert(r != NULL);

		qe_connections = qe_connections->next;

		shutdown_connection(r->socket);
		remove_connection(r->socket);

		count++;
	}

	LeaveCriticalSection(&csec_connections);

	log_debug_printf("done removing %d connection(s) in 'connections'", count);
}