#include "mapping.h"
#include "queue.h"
#include "connection.h"

queue_t				socket_to_recv_unicast_msg = { .head = NULL, .tail = NULL };
queue_t				socket_to_recv_multicast_msg = { .head = NULL, .tail = NULL };

CRITICAL_SECTION	csec_socket_to_recv_unicast_msg;
CRITICAL_SECTION	csec_socket_to_recv_multicast_msg;

socket_invokeid_mapping_ptr alloc_socket_invokeid_mapping(
	SOCKET socket, asn1SccUint invokeid_original, asn1SccUint invokeid_generated)
{
	socket_invokeid_mapping_ptr new_mapping = NULL;

	assert(socket > 0);
	assert(invokeid_original >= 0);
	assert(invokeid_generated >= 0);

	new_mapping = (socket_invokeid_mapping_ptr)memalloc(sizeof(socket_invokeid_mapping_t));
	if (new_mapping == NULL)
		return NULL;

	new_mapping->socket = socket;
	new_mapping->invokeid_original = invokeid_original;
	new_mapping->invokeid_generated = invokeid_generated;
	
	assert(new_mapping != NULL);

	return new_mapping;
}

void free_socket_invokeid_mapping(socket_invokeid_mapping_ptr p)
{
	if (p != NULL) {
		free(p);
		p = NULL;
	}
}

#define REMOVE_MAPPINGS(A) \
	void remove_##A##_mappings_for(SOCKET socket) { \
		queue_entry_ptr qe = NULL; \
		queue_entry_ptr qe_prev = NULL; \
		socket_invokeid_mapping_ptr p = NULL; \
		unsigned int count = 0; \
		\
		EnterCriticalSection(&csec_##A); \
		qe = A##.head; \
		while (qe != NULL) { \
			p = (socket_invokeid_mapping_ptr)(qe->ref); \
			assert(p != NULL); \
			\
			if (p->socket == socket) { \
				qe->ref = NULL; \
				qe_prev = qe; \
				qe = qe->next; \
				\
				q_free(&##A##, qe_prev); \
				free_socket_invokeid_mapping(p); \
				\
				count++; \
				continue; \
			} \
			qe = qe->next; \
		} \
		LeaveCriticalSection(&csec_##A); \
		\
		log_debug_printf("done removing %d mapping(s) in '%s' to closed socket: %d", count, #A, socket); \
	}

REMOVE_MAPPINGS(socket_to_recv_unicast_msg);
REMOVE_MAPPINGS(socket_to_recv_multicast_msg);

void remove_all_mappings_for(SOCKET socket)
{
	remove_socket_to_recv_unicast_msg_mappings_for(socket);
	remove_socket_to_recv_multicast_msg_mappings_for(socket);
}

void remove_all_mappings(void)
{
	queue_entry_ptr qe_connections = NULL;
	connection_ptr r = NULL;

	EnterCriticalSection(&csec_connections);

	qe_connections = connections.head;

	while (qe_connections != NULL)
	{
		r = (connection_ptr)qe_connections->ref;

		assert(r != NULL);

		qe_connections = qe_connections->next;

		remove_all_mappings_for(r->socket);
	}

	LeaveCriticalSection(&csec_connections);
}
