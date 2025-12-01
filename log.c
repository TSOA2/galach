#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "log.h"

void gh_panic(void) {
	exit(EXIT_FAILURE);
}

void gh_log(enum gh_log_type type, const char *m, ...) {
	va_list list;
	va_start(list, m);
	const char *prefix;
	switch (type) {
		case GH_LOG_ERR:  prefix = "error"; break;
		case GH_LOG_WARN: prefix = "warn"; break;
		case GH_LOG_INFO: prefix = "info"; break;
		default: prefix = "invalid"; break;
	}
	(void) fprintf(stderr, "[%s]: ", prefix);
	(void) vfprintf(stderr, m, list);
	(void) fputc('\n', stderr);
	(void) fflush(stderr);
	va_end(list);
}
