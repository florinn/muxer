#ifndef __MEM_H
#define __MEM_H

#include <memory.h>
#include "logging.h"
#include "common.h"

#define MEMZERO(A, B) memset(A, 0x00, B)

INLINE void* memalloc(int length)
{
	void* temp = malloc(length);
	if (temp)
		return temp;
	else
		log_debug_printf("memory allocation error");

	return NULL;
}

INLINE void* memrealloc(void* orig, int length)
{
	void *temp = realloc(orig, length);
	if (temp)
		return temp;
	else
		log_debug_printf("memory reallocation error");
	
	return NULL;
}

#endif // __MEM_H