#ifndef __MESSAGE_H
#define __MESSAGE_H

#include "protocol.h"
#include "winsock.h"
#include "queue.h"
#include "pdu.h"

typedef struct
{
	SOCKET			socket;
	pdu_ptr			pdu;
} in_message_t, *in_message_ptr;

typedef struct
{
	pdu_ptr			pdu;
} out_message_t, *out_message_ptr;

extern queue_t		r_in_messages;
extern queue_t		w_in_messages;
extern queue_t		r_out_messages;
extern queue_t		w_out_messages;

extern CRITICAL_SECTION		csec_r_in_messages;
extern CRITICAL_SECTION		csec_w_in_messages;
extern CRITICAL_SECTION		csec_r_out_messages;
extern CRITICAL_SECTION		csec_w_out_messages;

extern HANDLE h_event_in_messages;
extern HANDLE h_event_out_messages;

extern HANDLE h_event_new_message;

in_message_ptr	alloc_in_message(SOCKET socket, pdu_ptr pdu);
out_message_ptr	alloc_out_message(pdu_ptr pdu);

void free_in_message(in_message_ptr pdu);
void free_out_message(out_message_ptr pdu);

void free_r_in_messages(void);
void free_w_in_messages(void);
void free_r_out_messages(void);
void free_w_out_messages(void);

void remove_all_messages(void);

#endif // __MESSAGE_H