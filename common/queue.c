#include <stdio.h>
#include "queue.h"
#include "mem.h"

queue_entry_ptr q_alloc(void* p)
{
	queue_entry_ptr qe = NULL;

	assert(p != NULL);

	qe = (queue_entry_ptr)memalloc(sizeof(queue_entry_t));

	qe->next = NULL;
	qe->ref = p;

	assert(qe != NULL);

	return qe;
}

void q_free(queue_ptr q, queue_entry_ptr qe)
{
	queue_entry_ptr m_qe = NULL;
	queue_entry_ptr m_qe_prev = NULL;

	assert(q != NULL);
	assert(qe != NULL);

	if (q->head == NULL)
		assert(q->tail == NULL);

	if (q->head != NULL) {
		if (qe == q->head && qe == q->tail)	{
			assert(q->head == q->tail);
			q->head = q->tail = NULL;
		}
		else
			if (qe == q->head && qe != q->tail)	{
				assert(q->tail != NULL);
				q->head = q->head->next;
			}
			else
			{
				m_qe_prev = q->head;
				m_qe = q->head->next;

				while (m_qe != NULL) {
					assert(q->head != NULL);
					assert(q->tail != NULL);
					assert(m_qe_prev != NULL);

					if (m_qe == qe) {
						m_qe_prev->next = m_qe->next;
						m_qe = m_qe->next;
						continue;
					}

					m_qe_prev = m_qe;
					m_qe = m_qe->next;
				}

				if (qe != q->head && qe == q->tail)	{
					assert(q->head != NULL);
					assert(m_qe == NULL);
					assert(m_qe_prev != NULL);
					q->tail = m_qe_prev;
				}
			}
	}

	free(qe);
	qe = NULL;
}