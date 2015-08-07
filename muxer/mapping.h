#ifndef __MAPPING_H
#define __MAPPING_H

#include "winsock.h"
#include "queue.h"
#include "asn1crt.h"

typedef struct
{
	SOCKET			socket;
	asn1SccUint		invokeid_original;
	asn1SccUint		invokeid_generated;
} socket_invokeid_mapping_t, *socket_invokeid_mapping_ptr;

extern queue_t				socket_to_recv_unicast_msg;
extern queue_t				socket_to_recv_multicast_msg;

extern CRITICAL_SECTION		csec_socket_to_recv_unicast_msg;
extern CRITICAL_SECTION		csec_socket_to_recv_multicast_msg;

socket_invokeid_mapping_ptr alloc_socket_invokeid_mapping(
	SOCKET socket, asn1SccUint invokeid_original, asn1SccUint invokeid_generated);
void free_socket_invokeid_mapping(socket_invokeid_mapping_ptr p);
void remove_all_mappings_for(SOCKET socket);
void remove_all_mappings(void);

#endif // __MAPPING_H