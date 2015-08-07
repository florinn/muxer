#ifndef __PDU_H
#define __PDU_H

#define BUFFER_KSIZE 1024
#define PDU_HEADER_LENGTH 2

#include "asn1crt.h"
#include "queue.h"
#include "mem.h"

typedef struct
{
	unsigned char	buffer[BUFFER_KSIZE];
	size_t			length;
} pdu_t, *pdu_ptr;

#define DECLARE_FUNCTION_ENCODE_PDU(A,B) int encode_##A##_pdu(pdu_ptr* encoded_pdu, const B##* pdu_to_encode);
#define DECLARE_FUNCTION_DECODE_PDU(A,B) int decode_##A##_pdu(B##* decoded_pdu, const pdu_ptr pdu_to_decode);

#define ENCODE_PDU(A,B) \
	int encode_##A##_pdu(pdu_ptr* encoded_pdu, const B##* pdu_to_encode) { \
		assert(*encoded_pdu == NULL); \
		assert(pdu_to_encode != NULL); \
		\
		*encoded_pdu = alloc_pdu(); \
		char encode_buf[BUFFER_KSIZE]; \
		BitStream encoded_stream = { .buf = encode_buf, .count = BUFFER_KSIZE, .currentBit = 0, .currentByte = 0 }; \
		MEMZERO(encoded_stream.buf, encoded_stream.count); \
		int encode_err; \
		B##_Encode(pdu_to_encode, &encoded_stream, &encode_err, TRUE); \
		if (!encode_err) { \
			MEMZERO((*encoded_pdu)->buffer, encoded_stream.count); \
			memcpy_s((*encoded_pdu)->buffer, encoded_stream.count, encoded_stream.buf, encoded_stream.currentByte); \
			(*encoded_pdu)->length = (unsigned short)encoded_stream.currentByte; \
		} \
		return encode_err; \
	}

#define DECODE_PDU(A,B) \
	int decode_##A##_pdu(B##* decoded_pdu, const pdu_ptr pdu_to_decode) { \
		assert(pdu_to_decode != NULL); \
		\
		char decode_buf[BUFFER_KSIZE]; \
		BitStream decoded_stream = { .buf = decode_buf, .count = BUFFER_KSIZE, .currentBit = 0, .currentByte = 0 }; \
		MEMZERO(decoded_stream.buf, decoded_stream.count); \
		memcpy_s(decoded_stream.buf, decoded_stream.count, pdu_to_decode->buffer, pdu_to_decode->length); \
		int decode_err; \
		B##_Decode(decoded_pdu, &decoded_stream, &decode_err); \
		return decode_err; \
	}

void read_pdus_from_recv_buffer(unsigned char* recvbuf, const size_t recvbuflen, size_t* recvbufptr, queue_ptr pdus);
void write_pdu_to_send_buffer(const pdu_ptr pdu, unsigned char* sendbuf, const size_t sendbuflen, size_t* sendbufptr);

pdu_ptr alloc_pdu();
void free_pdu(pdu_ptr pdu);

#endif	// __PDU_H