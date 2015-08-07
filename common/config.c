#include <stdio.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "logging.h"
#include "mem.h"

#define MAX_BUF_LEN 128

char* read_line(FILE* file) 
{
	if (file == NULL) {
		log_error_printf("file pointer must be not null");
		exit(1);
	}

	int max_line_len = MAX_BUF_LEN;
	char* line_buffer = memalloc(sizeof(char) * max_line_len);
	if (line_buffer == NULL) 
		exit(1);

	char ch = getc(file);
	int count = 0;

	while ((ch != '\n') && (ch != EOF)) {
		if (count == max_line_len) {
			max_line_len += MAX_BUF_LEN;
			line_buffer = memrealloc(line_buffer, max_line_len);
			if (line_buffer == NULL)
				exit(1);
		}
		line_buffer[count++] = ch;
		ch = getc(file);
	}

	line_buffer[count] = '\0';
	char* line = memalloc(sizeof(char) * (count + 1));
	strncpy_s(line, count + 1, line_buffer, count + 1);
	
	free(line_buffer);
	
	return line;
}

BOOL split(const char* pair, const char* delim_token, char** key, char** value)
{
	char* delim = strstr(pair, delim_token);
	if (delim) {
		const size_t keyLen = delim - pair;
		const size_t delimLen = strlen(delim_token);
		const size_t valueLen = strlen(pair) - delimLen - keyLen;

		*key = memalloc(keyLen + 1);
		*value = memalloc(valueLen + 1);
		if (*key == NULL || *value == NULL)
			exit(1);

		strncpy_s(*key, keyLen + 1, pair, keyLen);
		strncpy_s(*value, valueLen + 1, delim + delimLen, valueLen);

		return TRUE;
	}
	else
		return FALSE;
}