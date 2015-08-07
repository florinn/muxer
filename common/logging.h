#ifndef __LOGGING_H
#define __LOGGING_H

#define DECLARE_FUNCTION_LOG(A)	int A##_printf(char* format, ...);

DECLARE_FUNCTION_LOG(log_info)
DECLARE_FUNCTION_LOG(log_debug)
DECLARE_FUNCTION_LOG(log_error)

void log_printf(char* format, va_list arg_list);

#define IMPLEMENT_FUNCTION_LOG(A,B) \
    int B##_printf(char * format, ...) { \
		va_list argList; \
		if(!##A##.##B) return 0; \
		va_start(argList, format); \
		log_printf(format, argList); \
		va_end(argList); \
		return 0; \
	}

#define MAX_LOG_FILENAME 256

#endif // __LOGGING_H