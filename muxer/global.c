#include "global.h"


IMPLEMENT_FUNCTION_LOG(global_config, log_info)
IMPLEMENT_FUNCTION_LOG(global_config, log_debug)
IMPLEMENT_FUNCTION_LOG(global_config, log_error)

#define CONFIG_FILENAME "muxer.config"

#define EQ_TOKEN "="
#define COL_TOKEN ":"

#define LOG_INFO	"LOG_INFO"
#define LOG_DEBUG	"LOG_DEBUG"
#define LOG_ERROR	"LOG_ERROR"
#define CONN_IN		"CONNECTION_IN"
#define CONN_OUT	"CONNECTION_OUT"


static void read_ip_address(const char* input, ip_address_ptr ip_address)
{
	char* ip = NULL;
	char* port = NULL;

	if (split(input, COL_TOKEN, &ip, &port)) {
		char* ip_trimmed = trim_ws(ip);
		char* port_trimmed = trim_ws(port);

		strcpy_s((*ip_address).ip, _countof((*ip_address).ip), ip_trimmed);
		(*ip_address).port = atoi(port_trimmed);

		free(ip);
		free(port);
	}
}

static void read_config_line(config_ptr config, const char* line)
{
	if (config == NULL) {
		log_error_printf("config pointer must be not null");
		exit(1);
	}

	char* key = NULL;
	char* value = NULL;
	if (split(line, EQ_TOKEN, &key, &value)) {
		char* key_trimmed = trim_ws(key);
		char* value_trimmed = trim_ws(value);

		if (!strcmp(key_trimmed, LOG_INFO))
			(*config).log_info = !strcmp(value_trimmed, STR(TRUE)) ? TRUE : FALSE;
		if (!strcmp(key_trimmed, LOG_DEBUG))
			(*config).log_debug = !strcmp(value_trimmed, STR(TRUE)) ? TRUE : FALSE;
		if (!strcmp(key_trimmed, LOG_ERROR))
			(*config).log_error = !strcmp(value_trimmed, STR(TRUE)) ? TRUE : FALSE;

		if (!strcmp(key_trimmed, CONN_IN)) 
			read_ip_address(value_trimmed, &config->connection_in);
		if (!strcmp(key_trimmed, CONN_OUT))
			read_ip_address(value_trimmed, &config->connection_out);

		free(key);
		free(value);
	}
}

void read_config(config_ptr config)
{
	FILE* config_file;
	fopen_s(&config_file, CONFIG_FILENAME, "r");
	if (!config_file) {
		log_error_printf("cannot open config file %s", CONFIG_FILENAME);
		exit(1);
	}

	while (!feof(config_file)) {
		char* line = read_line(config_file);
		read_config_line(config, line);
		free(line);
	}
}
