#ifndef __QUEUE_H
#define __QUEUE_H

#include <time.h>
#include "common.h"

typedef struct queue_entry_s
{
	struct queue_entry_s*	next;
	time_t					timestamp;
	void*					ref;
} queue_entry_t, *queue_entry_ptr;

typedef struct
{
	queue_entry_ptr	head;
	queue_entry_ptr tail;
} queue_t, *queue_ptr;


queue_entry_ptr q_alloc(void* p);

void q_free(queue_ptr q, queue_entry_ptr qe);

INLINE queue_entry_ptr q_put(queue_ptr q, queue_entry_ptr qe)
{
	time_t timestamp;
	queue_entry_ptr m_qe = NULL;

	assert(q != NULL);
	assert(qe != NULL);

	//verify unicity of qe
	m_qe = q->head;

	while (m_qe != NULL) {
		if (m_qe == qe)
			return m_qe;
		m_qe = m_qe->next;
	}
	//

	if (q->tail != NULL) {
		assert(q->head != NULL);
		q->tail->next = qe;
	}

	qe->next = NULL;
	q->tail = qe;

	if (q->head == NULL) {
		assert(q->tail != NULL);
		q->head = qe;
	}

	time(&timestamp);
	qe->timestamp = timestamp;

	return qe;
}

INLINE queue_entry_ptr q_get(queue_ptr q)
{
	queue_entry_ptr qe = NULL;

	assert(q != NULL);

	qe = q->head;

	if (q->tail == qe)
		q->tail = NULL;

	if (qe == NULL)
		return NULL;

	q->head = qe->next;

	return qe;
}

#endif // __QUEUE_H