#ifndef _GALACH_BYTECODE_H
#define _GALACH_BYTECODE_H

#include "types.h"
#include "ast.h"

typedef enum {
	GH_SYSFUN_PRINT8 = 0,
	GH_SYSFUN_PRINT16,
	GH_SYSFUN_PRINT32,
	GH_SYSFUN_PRINT64,
	GH_SYSFUN_LAST,
} gh_sysfun_idx;

typedef struct {
	u64 offset;
	u64 nbytes;
} gh_fun;

DEFINE_VEC(gh_fun);
DEFINE_VEC(u8);

typedef struct {
	VEC(u8) bytes;
	VEC(gh_fun) funs;
	u64 main_idx; // idx into funs
	u8 main_defined;
} gh_bytecode;

// beware of double evaluation
#define emitb_vec(v, b) do { \
	GROW_VEC(v, 1); \
	APPEND_VEC_RAW(v, b); \
} while (0)

#define emitw_vec(v, w) do { \
	GROW_VEC(v, 2); \
	APPEND_VEC_RAW(v, (w >> 8) & 0xff); \
	APPEND_VEC_RAW(v, (w >> 0) & 0xff); \
} while (0)

#define emitdw_vec(v, dw) do { \
	GROW_VEC(v, 4); \
	APPEND_VEC_RAW(v, (dw >> 24) & 0xff); \
	APPEND_VEC_RAW(v, (dw >> 16) & 0xff); \
	APPEND_VEC_RAW(v, (dw >> 8) & 0xff); \
	APPEND_VEC_RAW(v, (dw >> 0) & 0xff); \
} while (0)

#define emitqw_vec(v, qw) do { \
	GROW_VEC(v, 8); \
	APPEND_VEC_RAW(v, (qw >> 56) & 0xff); \
	APPEND_VEC_RAW(v, (qw >> 48) & 0xff); \
	APPEND_VEC_RAW(v, (qw >> 40) & 0xff); \
	APPEND_VEC_RAW(v, (qw >> 32) & 0xff); \
	APPEND_VEC_RAW(v, (qw >> 24) & 0xff); \
	APPEND_VEC_RAW(v, (qw >> 16) & 0xff); \
	APPEND_VEC_RAW(v, (qw >> 8) & 0xff); \
	APPEND_VEC_RAW(v, (qw >> 0) & 0xff); \
} while (0)

// Should implement gh_bytecode_verify
// that checks if the bytecode is valid; that is if
// all indices are within bounds, as well as other stuff.
void gh_bytecode_init(gh_bytecode *bytecode);
int gh_bytecode_src(gh_bytecode *bytecode, char *file);
void gh_bytecode_deinit(gh_bytecode *bytecode);
#endif // _GALACH_BYTECODE_H
