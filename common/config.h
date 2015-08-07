#ifndef __CONFIG_H
#define __CONFIG_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <stdio.h>

#define MAX_IP_ADDR_LEN 16

typedef struct
{
	char	ip[MAX_IP_ADDR_LEN];
	USHORT	port;
} ip_address_t, *ip_address_ptr;

char* read_line(FILE* file);
BOOL split(const char* pair, const char* delim_token, char** key, char** value);

#endif	// __CONFIG_H