#ifndef _GALACH_BYTECODE_H
#define _GALACH_BYTECODE_H

#include "types.h"
#include "ast.h"

typedef struct {
	u64 offset;
	u64 nbytes;
} gh_fun;

DEFINE_VEC(gh_fun);
DEFINE_VEC(u8);

typedef struct {
	VEC(u8) bytes;
	VEC(gh_fun) funs;
	u64 main;
} gh_bytecode;

// Should implement gh_bytecode_verify
// that checks if the bytecode is valid; that is if
// all indices are within bounds, as well as other stuff.
void gh_bytecode_init(gh_bytecode *bytecode);
void gh_bytecode_mem(gh_bytecode *bytecode, gh_ast *ast);
int gh_bytecode_src(gh_bytecode *bytecode, char *file);
void gh_bytecode_deinit(gh_bytecode *bytecode);
#endif // _GALACH_BYTECODE_H
