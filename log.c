#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include "log.h"

struct {
	FILE *file;
	const char *filename;
	log_type minlevel;
} logs;
static void log_init() __attribute__((constructor));

static const char *log_prefix[] = {
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR"
};

static void log_init() {
	logs.file = stderr;
	logs.filename = NULL;
	logs.minlevel = LOG_INFO;

	char *estr;
	char *eptr;
	if (estr = getenv("DSKTOOLS_LOGLEVEL")) {
		int level = (int) strtol(estr, &eptr, 10);
		if (eptr == estr + strlen(estr)) {
			log_minlevel_set(level);
		}
	}
	if (estr = getenv("DSKTOOLS_LOGFILE")) {
		log_file_set(estr);
	}
}

void log_destroy() {
	if (logs.file && logs.file != stderr) {
		fclose(logs.file);
		logs.file = NULL;
	}
	if (logs.filename) {
		free((void*)logs.filename);
	}
}

void log_file_set(const char *logfile) {
	if (logfile == NULL) {
		logs.file = NULL;
		logs.filename = NULL;
	} else if (strcmp(logfile, "stderr") == 0) {
		logs.file = stderr;
		logs.filename = NULL;
	} else {
		logs.file = fopen(logfile, "a");
		if (logs.file == NULL) {
			fprintf(stderr, "Error setting logfile %s: %s\n",
				logfile, strerror(errno));
			logs.file = stderr;
			logs.filename = NULL;
		} else {
			logs.filename = strdup(logfile);
			atexit(log_destroy);
		}
	}
}

void log_minlevel_set(log_type minlevel) {
	logs.minlevel = minlevel;
}

static const char *log_prefix_get(log_type type) {
	return log_prefix[type];
}

void log_write(log_type type, const char *file, int line,
               const char *fmt, ...) {
	va_list ap;
	char buffer[2048];
	time_t secs;
	struct tm tstruct;
	
	if (!logs.file) return;
	if (logs.minlevel > type) return;
	
	va_start(ap, fmt);
	vsnprintf(buffer, 2047, fmt, ap);
	secs = time(0);
	localtime_r(&secs, &tstruct);
  
	fprintf(logs.file,
		"[%s][%s:%d][%04u-%02u-%02u %02u:%02u:%02u] %s\n",
		log_prefix_get(type),
		file, line,
		tstruct.tm_year+1900, tstruct.tm_mon, tstruct.tm_mday,
		tstruct.tm_hour,tstruct.tm_min,tstruct.tm_sec,
		buffer);
	fflush(logs.file);
}

