#ifndef _GALACH_LOG_H
#define _GALACH_LOG_H

enum gh_log_type {
	GH_LOG_ERR,
	GH_LOG_WARN,
	GH_LOG_INFO,
};

void gh_panic(void);
void gh_log(enum gh_log_type type, const char *m, ...);

#endif // _GALACH_LOG_H
