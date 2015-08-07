#include "protocol.h"
#include "global.h"
#include "queue.h"
#include "message.h"
#include "messages.h"
#include "mapping.h"
#include "connection.h"

#define MSG_LOG(RECV_SEND, FROM_CLIENT, FROM_SKT, ORIGIID_VAL, TO_CLIENT, TO_SKT, GENIID_VAL, INFO) \
		"[%s]\t [from %s:%d] [to %s:%d] [origIID val: %d] [genIID val: %d] [Info: %s]" \
		, RECV_SEND, FROM_CLIENT, FROM_SKT, TO_CLIENT, TO_SKT, ORIGIID_VAL, GENIID_VAL, INFO 


static void put_into_socket_to_recv_unicast_msg(const SOCKET socket, const asn1SccUint invokeid, const pdu_ptr pdu);

DECLARE_FUNCTION_ENCODE_PDU(in, InputType);
DECLARE_FUNCTION_ENCODE_PDU(out, OutputType);

DECLARE_FUNCTION_DECODE_PDU(in, InputType);
DECLARE_FUNCTION_DECODE_PDU(out, OutputType);

static void copy_pdu(pdu_ptr* to, const pdu_ptr from);

#define DECLARE_FUNCTION_COPY_MESSAGE(A) static void copy_##A##_message(A##_message_ptr* to, const A##_message_ptr from);

DECLARE_FUNCTION_COPY_MESSAGE(in);
DECLARE_FUNCTION_COPY_MESSAGE(out);

static void put_pdu_into_w_in_messages(const SOCKET socket, const pdu_ptr pdu);
static void put_pdu_into_w_out_messages(const pdu_ptr pdu);

asn1SccUint invokeid_gen_counter = 0;

extern connection_t	out_connection;


void process_incoming_in_messages()
{
	queue_entry_ptr m_qe = NULL;
	in_message_ptr r = NULL, tmp = NULL;

	// read message from r_in_messages
	EnterCriticalSection(&csec_r_in_messages);
	
	m_qe = q_get(&r_in_messages);
	if (m_qe != NULL) {
		tmp = (in_message_ptr)(m_qe->ref);
		assert(tmp != NULL);
		
		q_free(&r_in_messages, m_qe);
	
		copy_in_message(&r, tmp);
	
		free_in_message(tmp);
	
		LeaveCriticalSection(&csec_r_in_messages);
	}
	else {
		LeaveCriticalSection(&csec_r_in_messages);
		return;
	}

	// decode & process message based on its type
	InputType decoded_in_pdu;
	decode_in_pdu(&decoded_in_pdu, r->pdu);
	asn1SccUint invokeid_original = decoded_in_pdu.invokeid;

	log_debug_printf(
		MSG_LOG(/*RECV_SEND*/"recv"
		, /*FROM_CLIENT*/"client", /*FROM_SKT*/r->socket
		, /*ORIGIID_VAL*/(int)invokeid_original
		, /*TO_CLIENT*/"n/a", /*TO_SKT*/-1
		, /*GENIID_VAL*/-1
		, /*INFO*/"n/a")
		);
	
	// create mapping
	put_into_socket_to_recv_unicast_msg(r->socket, invokeid_original, r->pdu);
	
	// generate invokeid
	decoded_in_pdu.invokeid = invokeid_gen_counter;

	// reencode pdu with original invokeid
	pdu_ptr reencoded_pdu = NULL;
	int err_res = encode_in_pdu(&reencoded_pdu, &decoded_in_pdu);
	if (err_res != 0) {
		log_debug_printf(
			MSG_LOG(/*RECV_SEND*/"send"
				, /*FROM_CLIENT*/"client", /*FROM_SKT*/r->socket
				, /*ORIGIID_VAL*/(int)invokeid_original
				, /*TO_CLIENT*/"server", /*TO_SKT*/out_connection.socket
				, /*GENIID_VAL*/(int)decoded_in_pdu.invokeid
				, /*INFO*/"!!! ERROR when re-encoding PDU !!!")
			);

		free_in_message(r);
		return;
	}

	// put the message in w_out_messages
	put_pdu_into_w_out_messages(reencoded_pdu);
	
	log_debug_printf(
		MSG_LOG(/*RECV_SEND*/"send"
			, /*FROM_CLIENT*/"client", /*FROM_SKT*/r->socket
			, /*ORIGIID_VAL*/(int)invokeid_original
			, /*TO_CLIENT*/"server", /*TO_SKT*/out_connection.socket
			, /*GENIID_VAL*/(int)decoded_in_pdu.invokeid
			, /*INFO*/"n/a")
		);

	free_in_message(r);
}

static asn1SccUint generate_invokeid(void)
{
	const static int MAX_INVOKEID = 32767;
	invokeid_gen_counter = invokeid_gen_counter % MAX_INVOKEID + 1;
	return invokeid_gen_counter;
}

static void put_into_socket_to_recv_unicast_msg(const SOCKET socket, const asn1SccUint invokeid, const pdu_ptr pdu)
{
	socket_invokeid_mapping_ptr new_mapping = NULL;
	queue_entry_ptr new_queue_entry = NULL;

	assert(pdu != NULL);

	EnterCriticalSection(&csec_socket_to_recv_unicast_msg);

	new_mapping = alloc_socket_invokeid_mapping(socket, invokeid, generate_invokeid());
	new_queue_entry = q_alloc((void*)new_mapping);
	q_put(&socket_to_recv_unicast_msg, new_queue_entry);
	
	LeaveCriticalSection(&csec_socket_to_recv_unicast_msg);
}

void process_incoming_out_messages() 
{
	out_message_ptr p = NULL, tmp = NULL;
	pdu_ptr recv_pdu = NULL;
	queue_entry_ptr m_qe = NULL, m_qe_assoc = NULL, m_qe_assoc_prev = NULL;

	socket_invokeid_mapping_ptr s;

	// read message from r_out_messages 
	EnterCriticalSection(&csec_r_out_messages);

	m_qe = q_get(&r_out_messages);
	if (m_qe != NULL) {
		tmp = (out_message_ptr)(m_qe->ref);
		assert(tmp != NULL);
		
		q_free(&r_out_messages, m_qe);

		copy_out_message(&p, tmp);

		free_out_message(tmp);

		LeaveCriticalSection(&csec_r_out_messages);
	}
	else {
		LeaveCriticalSection(&csec_r_out_messages);
		return;
	}

	// decode & process message based on its type
	OutputType decoded_out_pdu; 
	decode_out_pdu(&decoded_out_pdu, p->pdu);
	asn1SccUint invokeid_generated = decoded_out_pdu.invokeid;

	EnterCriticalSection(&csec_socket_to_recv_unicast_msg);
	
	// put the message into w_in_messages of the socket mapped to invokeid
	m_qe_assoc = socket_to_recv_unicast_msg.head;
	
	while(m_qe_assoc != NULL) {
		s = (socket_invokeid_mapping_ptr)(m_qe_assoc->ref);
	
		assert(s != NULL);
	
		if (s->invokeid_generated == invokeid_generated) {
			// reencode pdu with original invokeid
			decoded_out_pdu.invokeid = s->invokeid_original;
			pdu_ptr reencoded_pdu = NULL;
			encode_out_pdu(&reencoded_pdu, &decoded_out_pdu);

			// create message and put it into w_in_messages
			copy_pdu(&recv_pdu, reencoded_pdu);
	
			put_pdu_into_w_in_messages(s->socket, recv_pdu);
	
			free_pdu(reencoded_pdu);

			log_debug_printf(
				MSG_LOG(/*RECV_SEND*/"send"
					, /*FROM_CLIENT*/"server", /*FROM_SKT*/out_connection.socket
					, /*ORIGIID_VAL*/(int)s->invokeid_original
					, /*TO_CLIENT*/"client", /*TO_SKT*/s->socket
					, /*GENIID_VAL*/(int)s->invokeid_generated
					, /*INFO*/"PDU sent to client with original invoke ID")
					);

			m_qe_assoc->ref = NULL;
			m_qe_assoc_prev = m_qe_assoc;
			m_qe_assoc = m_qe_assoc->next;

			// delete mapping btw socket and invokeid
			q_free(&socket_to_recv_unicast_msg, m_qe_assoc_prev);
			free_socket_invokeid_mapping(s);
			
			continue;
		}
	
		m_qe_assoc = m_qe_assoc->next;
	}
	
	free_out_message(p);
	
	LeaveCriticalSection(&csec_socket_to_recv_unicast_msg);
}

ENCODE_PDU(in, InputType);
ENCODE_PDU(out, OutputType);

DECODE_PDU(in, InputType);
DECODE_PDU(out, OutputType);

static void copy_pdu(pdu_ptr* to, const pdu_ptr from)
{
	(*to) = memalloc(sizeof(pdu_t));
	if ((*to) == NULL)
		exit(1);

	MEMZERO((*to)->buffer, from->length);
	memcpy_s((*to)->buffer, _countof((*to)->buffer), from->buffer, from->length);
	(*to)->length = from->length;
}

static void copy_out_message(out_message_ptr* to, const out_message_ptr from)
{
	pdu_ptr recv_pdu = NULL;

	assert(from != NULL);

	copy_pdu(&recv_pdu, from->pdu);

	(*to) = alloc_out_message(recv_pdu);

	assert((*to) != NULL);
}

static void copy_in_message(in_message_ptr* to, const in_message_ptr from)
{
	pdu_ptr recv_pdu = NULL;

	assert(from != NULL);

	copy_pdu(&recv_pdu, from->pdu);

	(*to) = alloc_in_message(from->socket, recv_pdu);

	assert((*to) != NULL);
}

static void put_pdu_into_w_in_messages(const SOCKET socket, const pdu_ptr pdu)
{
	in_message_ptr new_in_message = NULL;
	queue_entry_ptr new_q_entry = NULL;

	new_in_message = alloc_in_message(socket, pdu);
	new_q_entry = q_alloc((void*)new_in_message);
	
	EnterCriticalSection(&csec_w_in_messages);
	q_put(&w_in_messages, new_q_entry);
	LeaveCriticalSection(&csec_w_in_messages);

	SetEvent(h_event_new_message);
}

static void put_pdu_into_w_out_messages(const pdu_ptr pdu)
{
	out_message_ptr new_out_message = NULL;
	queue_entry_ptr new_queue_entry = NULL;

	new_out_message = alloc_out_message(pdu);
	new_queue_entry = q_alloc((void*)new_out_message);
	
	EnterCriticalSection(&csec_w_out_messages);
	q_put(&w_out_messages, new_queue_entry);
	LeaveCriticalSection(&csec_w_out_messages);

	SetEvent(h_event_new_message);
}