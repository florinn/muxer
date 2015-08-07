#include <stdio.h>
#include <direct.h>
#include <share.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include "logging.h"
#include "common.h"

#define MAX_LOG_TIME_LEN	40
#define MAX_LOG_ROW_LEN		512

#define MAX_LOG_SIZE		10485760
#define MAX_NO_LOG_FILES	10

extern const char LOG_PATH[MAX_LOG_FILENAME];
extern const char LOG_FILENAME[MAX_LOG_FILENAME];
extern char LOG_PATH_FILENAME[MAX_LOG_FILENAME * 2 + 1];
extern const char LOG_INDEX[MAX_LOG_FILENAME];
extern char LOG_PATH_INDEX[MAX_LOG_FILENAME * 2 + 1];

FILE* log_file = NULL;

static int reset_log_file()
{
	int index = 0;
	FILE* findex;
	char savedlog[MAX_LOG_FILENAME];

	fclose(log_file);
	fopen_s(&findex, LOG_PATH_INDEX,"r");
	if(findex)
		fscanf_s(findex,"%d",&index);
	
	index++;
	sprintf_s(savedlog, MAX_LOG_FILENAME, "%s.%d", LOG_PATH_FILENAME, index);
	
	rename(LOG_PATH_FILENAME, savedlog);
	
	if(findex) 
		fclose(findex);
	
	fopen_s(&findex, LOG_PATH_INDEX, "w");
	if(findex){
		fprintf(findex,"%d", index);
		fclose(findex);
	}
	
	sprintf_s(savedlog, MAX_LOG_FILENAME, "%s.%d", LOG_PATH_FILENAME, index - MAX_NO_LOG_FILES);
	
	remove(savedlog);
	
	log_file = _fsopen(LOG_PATH_FILENAME, "w", _SH_DENYWR);
	
	return 0;
}

void log_printf(char* format, va_list arg_list)
{
	long fsize;

	SYSTEMTIME sys_time;
	GetSystemTime(&sys_time);
	
	if (!log_file)
	{
		_mkdir(LOG_PATH);
		log_file = _fsopen(LOG_PATH_FILENAME, "a", _SH_DENYWR);
	}
	
	char curr_time[MAX_LOG_TIME_LEN];
	sprintf_s(curr_time, MAX_LOG_TIME_LEN, "[%02d/%02d/%d %02d:%02d:%02d %03d]\t",
		(int)sys_time.wDay, (int)sys_time.wMonth, (int)sys_time.wYear,
		(int)sys_time.wHour, (int)sys_time.wMinute, (int)sys_time.wSecond,
		(int)sys_time.wMilliseconds);

	char new_format[MAX_LOG_ROW_LEN];
	sprintf_s(new_format, MAX_LOG_ROW_LEN, "%s %s\n", curr_time, format);

	vfprintf(stdout, new_format, arg_list);
	vfprintf(log_file, new_format, arg_list);
	fflush(log_file);
	
	fsize = ftell(log_file);
	if (fsize != -1 && fsize > MAX_LOG_SIZE)
		reset_log_file();
}