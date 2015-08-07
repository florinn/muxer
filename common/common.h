#ifndef __COMMON_H
#define __COMMON_H

#define INLINE __forceinline

#define M_CONC(A, B) M_CONC_(A, B)
#define M_CONC_(A, B) A##B

#define STR(A) #A

#include <stdlib.h>
#include <assert.h>

#endif	// __COMMON_H