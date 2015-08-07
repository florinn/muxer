#include "pdu.h"
#include "mem.h"
#include "winsock.h"

void read_pdus_from_recv_buffer(unsigned char* recvbuf, const size_t recvbuflen, size_t* recvbufptr, queue_ptr pdus)
{
	unsigned short us_hLen = 0, us_nLen = 0;
	pdu_ptr recv_pdu = NULL;
	queue_entry_ptr new_queue_entry = NULL;

	assert(*recvbufptr >= 0);
	assert(*recvbufptr <= recvbuflen);

	if (*recvbufptr < PDU_HEADER_LENGTH)
		// no more space in buffer
		return;

	memcpy_s(&us_nLen, sizeof(unsigned short), recvbuf, PDU_HEADER_LENGTH);

	us_hLen = ntohs(us_nLen);

	assert(us_hLen <= (recvbuflen - PDU_HEADER_LENGTH));

	while ((*recvbufptr >= PDU_HEADER_LENGTH) &&
		(us_hLen > 0 && us_hLen <= *recvbufptr - PDU_HEADER_LENGTH)) {

		recv_pdu = (pdu_ptr)memalloc(sizeof(pdu_t));

		recv_pdu->length = (long)us_hLen;
		MEMZERO(recv_pdu->buffer, recv_pdu->length);

		// copy data into pdu
		memcpy_s(recv_pdu->buffer, recv_pdu->length, recvbuf + PDU_HEADER_LENGTH, recv_pdu->length);

		// move pointer in buffer
		*recvbufptr -= recv_pdu->length + PDU_HEADER_LENGTH;
		memmove_s(recvbuf, recvbuflen, recvbuf + recv_pdu->length + PDU_HEADER_LENGTH, *recvbufptr);

		assert(*recvbufptr >= 0);
		assert(*recvbufptr <= recvbuflen);

		new_queue_entry = q_alloc((void*)recv_pdu);

		q_put(pdus, new_queue_entry);


		if (*recvbufptr >= PDU_HEADER_LENGTH) {
			memcpy_s(&us_nLen, sizeof(unsigned short), recvbuf, PDU_HEADER_LENGTH);
			us_hLen = ntohs(us_nLen);
			assert(us_hLen <= (recvbuflen - PDU_HEADER_LENGTH));
		}
		else
			break;
	}
}

void write_pdu_to_send_buffer(const pdu_ptr pdu, unsigned char* sendbuf, const size_t sendbuflen, size_t* sendbufptr)
{
	assert(pdu != NULL);
	assert(PDU_HEADER_LENGTH + pdu->length <= sendbuflen);
	assert(pdu->length >= PDU_HEADER_LENGTH);

	MEMZERO(sendbuf, pdu->length);

	// pdu length
	unsigned short h_len = (unsigned short)pdu->length;
	unsigned short n_len = htons(h_len);

	memcpy_s(sendbuf, sendbuflen, &n_len, PDU_HEADER_LENGTH);

	memcpy_s(sendbuf + PDU_HEADER_LENGTH, sendbuflen, pdu->buffer, pdu->length);

	*sendbufptr = PDU_HEADER_LENGTH + pdu->length;

	assert(*sendbufptr >= 0);
	assert(*sendbufptr <= sendbuflen);
}

pdu_ptr alloc_pdu() 
{
	pdu_ptr pdu = memalloc(sizeof(pdu_t));
	if (pdu == NULL)
		exit(1);
	MEMZERO(pdu->buffer, _countof(pdu->buffer));
	pdu->length = 0;
	return pdu;
}

void free_pdu(pdu_ptr pdu) 
{
	free(pdu);
	pdu = NULL;
}