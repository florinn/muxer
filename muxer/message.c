#include "message.h"
#include "global.h"

queue_t		r_in_messages = { .head = NULL, .tail = NULL };
queue_t		w_in_messages = { .head = NULL, .tail = NULL };
queue_t		r_out_messages = { .head = NULL, .tail = NULL };
queue_t		w_out_messages = { .head = NULL, .tail = NULL };

CRITICAL_SECTION	csec_r_in_messages;
CRITICAL_SECTION	csec_w_in_messages;
CRITICAL_SECTION	csec_r_out_messages;
CRITICAL_SECTION	csec_w_out_messages;


out_message_ptr alloc_out_message(pdu_ptr pdu)
{
	out_message_ptr new_message = NULL;

	assert(pdu != NULL);

	new_message = memalloc(sizeof(out_message_t));
	if (new_message == NULL)
		return NULL;

	new_message->pdu = pdu;

	assert(new_message != NULL);

	return new_message;
}

in_message_ptr alloc_in_message(SOCKET socket, pdu_ptr pdu)
{
	in_message_ptr new_message = NULL;

	assert(socket > 0);
	assert(pdu != NULL);

	new_message = memalloc(sizeof(in_message_t));
	if (new_message == NULL)
		return NULL;

	new_message->socket = socket;
	new_message->pdu = pdu;

	assert(new_message != NULL);

	return new_message;
}


#define FREE_MESSAGE(A) \
	void free_##A##_message(A##_message_ptr msg) { \
		if (msg != NULL) { \
			if (msg->pdu != NULL) { \
				free_pdu(msg->pdu); \
				msg->pdu = NULL; \
			} \
			free(msg); \
			msg = NULL; \
		} \
	}

FREE_MESSAGE(in)
FREE_MESSAGE(out)


#define FREE_Q_MESSAGES(A,B) \
	void free_##A##_##B##_messages(void) { \
		queue_entry_ptr qe = NULL; \
		B##_message_ptr tmp = NULL; \
		EnterCriticalSection(&csec_##A##_##B##_messages); \
		while (A##_##B##_messages.head != NULL) { \
			qe = q_get(&##A##_##B##_messages); \
			if (qe != NULL) { \
				tmp = (B##_message_ptr)(qe->ref); \
				assert(tmp != NULL); \
				q_free(&##A##_##B##_messages, qe); \
				free_##B##_message(tmp); \
			} \
		} \
		LeaveCriticalSection(&csec_##A##_##B##_messages); \
	}

FREE_Q_MESSAGES(r,in)
FREE_Q_MESSAGES(w,in)
FREE_Q_MESSAGES(r,out)
FREE_Q_MESSAGES(w,out)


void remove_all_messages(void)
{
	free_r_in_messages();
	free_w_in_messages();
	free_r_out_messages();
	free_w_out_messages();
}