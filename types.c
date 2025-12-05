#include "types.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void *gh_malloc(u64 s) {
	void *p = malloc((size_t) s);
	if (!p) {
		gh_log(GH_LOG_ERR, "malloc failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	return p;
}

void *gh_realloc(void *old, u64 s) {
	void *p = realloc(old, (size_t) s);
	if (!p) {
		gh_log(GH_LOG_ERR, "realloc failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	return p;
}

void gh_free(void *p) {
	if (LIKELY(p))
		free(p);
}
