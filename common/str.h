#ifndef __STR_H
#define __STR_H

#include <string.h>
#include <ctype.h>
#include "common.h"

INLINE char* trim_ws(char *str)
{
	while (isspace(*str)) str++;

	char* end = str + strlen(str) - 1;
	while (end > str && isspace(*end)) end--;
	*(end + 1) = '\0';

	return str;
}

#endif // __STR_H