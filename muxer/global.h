#ifndef __GLOBAL_H
#define __GLOBAL_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include "common.h"
#include "mem.h"
#include "str.h"
#include "logging.h"
#include "config.h"
#include "pdu.h"

typedef struct
{
	ip_address_t connection_in;
	ip_address_t connection_out;

	BOOL log_info;
	BOOL log_debug;
	BOOL log_error;
} config_t, *config_ptr;

extern config_t global_config;
void read_config(config_ptr);

#endif	// __GLOBAL_H