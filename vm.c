#include <stdlib.h>
#include "vm.h"
#include "log.h"

void gh_vm_init(gh_vm *vm, gh_bytecode *bytecode) {
	(void) vm; (void) bytecode;
}

void gh_vm_run(gh_vm *vm) {
	(void) vm;
}

void gh_vm_debug(FILE *fp, gh_vm *vm) {
	(void) fp; (void) vm;
}

void gh_vm_deinit(gh_vm *vm) {
	(void) vm;
}

#if 0
// IMPL: gh_vm_alloc()
// IMPL: gh_vm_realloc()
// IMPL: CHECK()

static void *gh_vm_alloc(len size) {
	void *p = calloc(1, size);
	if (UNLIKELY(!p)) {
		gh_log(GH_LOG_ERR, "vm: out of memory");
		exit(EXIT_FAILURE);
	}
	return p;
}

static const len stack_bs = 1024;
void gh_vm_init(gh_vm *vm, gh_bytecode *bytecode) {
	*vm = (gh_vm) {
		.bc = bytecode,
		.stack = {
			.b = gh_vm_alloc(stack_bs),
			.size = stack_bs,
		},
	};
}

static void gh_vm_setip_fun(gh_vm *vm, gh_function *fun) {
	gh_cpage *cpage = NULL;
	CHECK(cpage, vm->cpages, fun->cpage, vm->ncpages);
	if (UNLIKELY(!cpage)) {
		gh_log(GH_LOG_ERR, "vm: no such cpage(" IDX_F ")", fun->cpage);
		exit(EXIT_FAILURE);
	}

	vm->ip = cpage->offset;
}

// Notes:
//
// --
// GCC is probably way better at optimizing this into a jumptable
// than I would be (verified w/ Godbolt), so I'll leave it as a switch.
//
// Skimming through Lua's implementation of it's VM, it looks like they use
// some basic macros to convert switch cases to goto labels, so I might do that
// to make migration easier if it seems a manual jmptable is a better option
// in the future
//
// But the goal of this project isn't to be extremely performant and
// I should probably try to avoid premature optimization.
#if 1
#define SWITCH(c) switch (c)
#define CASE(c) case c:
#else
// define these if some day I decided to use a jumptable
#define SWITCH(c)
#define CASE(c)
#endif

// Endianness doesn't matter here considering all this is happening
// on the same machine.
#define POP(type, place) do { \
	if (UNLIKELY(sizeof(type) > vm->stack.used)) { \
		gh_log(GH_LOG_ERR, "vm: stack pointer tried to go negative!"); \
		exit(EXIT_FAILURE); \
	} \
	vm->stack.used -= sizeof(type); \
	place = *(type *)&vm->stack.b[vm->stack.used]; \
} while (0)

#define PUSH(type, place) do { \
	if ((vm->stack.used+sizeof(type)) >= vm->stack.size) { \
		vm->stack.size += stack_bs; \
		vm->stack.b = gh_realloc(vm->stack.b, vm->stack.size); \
	} \
	*(type *)&vm->stack.b[vm->stack.used] = place; \
	vm->stack.used += sizeof(type); \
} while (0)

#define ADD(type) do { \
	type one, two; \
	POP(type, two); POP(type, one); \
	PUSH(type, one+two); \
} while (0)

#define SUB(type) do { \
	type one, two; \
	POP(type, two); POP(type, one); \
	PUSH(type, two-one); \
} while (0)

#define MUL(type) do { \
	type one, two; \
	POP(type, two); POP(type, one); \
	PUSH(type, one*two); \
} while (0)

#define IDIV(type) do { \
	type one, two; \
	POP(type, two); POP(type, one); \
	PUSH(type, two%one); \
	PUSH(type, two/one); \
} while (0)

#define DIV(type) do { \
	type one, two; \
	POP(type, two); POP(type, one); \
	PUSH(type, two/one); \
} while (0)

void gh_vm_run(gh_vm *vm) {
	gh_function *main = NULL;
	CHECK(main, vm->functions, vm->main_fun, vm->nfunctions);
	if (UNLIKELY(!main)) {
		gh_log(GH_LOG_ERR, "vm: no main function!");
		exit(EXIT_FAILURE);
	}
	gh_vm_setip_fun(vm, main);
	u8 *const e = c + vm->bc->ncode + 1;
	for ( ;; ) {
		u8 *c = &vm->bc->code[vm->ip];
		if (c == e)
			break;

		SWITCH (*c) {
			CASE(GH_OP_ADD_I8):
			CASE(GH_OP_ADD_U8):
			CASE(GH_OP_ADD_I16):
			CASE(GH_OP_ADD_U16):
			CASE(GH_OP_ADD_I32):
			CASE(GH_OP_ADD_U32):
			CASE(GH_OP_ADD_I64):
			CASE(GH_OP_ADD_U64):
			CASE(GH_OP_ADD_F32):
			CASE(GH_OP_ADD_F64):

			CASE(GH_OP_SUB_I8):
			CASE(GH_OP_SUB_U8):
			CASE(GH_OP_SUB_I16):
			CASE(GH_OP_SUB_U16):
			CASE(GH_OP_SUB_I32):
			CASE(GH_OP_SUB_U32):
			CASE(GH_OP_SUB_I64):
			CASE(GH_OP_SUB_U64):
			CASE(GH_OP_SUB_F32):
			CASE(GH_OP_SUB_F64):

			CASE(GH_OP_MUL_I8):
			CASE(GH_OP_MUL_U8):
			CASE(GH_OP_MUL_I16):
			CASE(GH_OP_MUL_U16):
			CASE(GH_OP_MUL_I32):
			CASE(GH_OP_MUL_U32):
			CASE(GH_OP_MUL_I64):
			CASE(GH_OP_MUL_U64):
			CASE(GH_OP_MUL_F32):
			CASE(GH_OP_MUL_F64):

			CASE(GH_OP_DIV_I8):
			CASE(GH_OP_DIV_U8):
			CASE(GH_OP_DIV_I16):
			CASE(GH_OP_DIV_U16):
			CASE(GH_OP_DIV_I32):
			CASE(GH_OP_DIV_U32):
			CASE(GH_OP_DIV_I64):
			CASE(GH_OP_DIV_U64):
			CASE(GH_OP_DIV_F32):
			CASE(GH_OP_DIV_F64):

			CASE(GH_OP_CMP_I8):
			CASE(GH_OP_CMP_U8):
			CASE(GH_OP_CMP_I16):
			CASE(GH_OP_CMP_U16):
			CASE(GH_OP_CMP_I32):
			CASE(GH_OP_CMP_U32):
			CASE(GH_OP_CMP_I64):
			CASE(GH_OP_CMP_U64):
			CASE(GH_OP_CMP_F32):
			CASE(GH_OP_CMP_F64):

			CASE(GH_TOK_BNEG8):
			CASE(GH_TOK_BNEG16):
			CASE(GH_TOK_BNEG32):
			CASE(GH_TOK_BNEG64):

			CASE(GH_TOK_BAND8):
			CASE(GH_TOK_BAND16):
			CASE(GH_TOK_BAND32):
			CASE(GH_TOK_BAND64):

			CASE(GH_TOK_BOR8):
			CASE(GH_TOK_BOR16):
			CASE(GH_TOK_BOR32):
			CASE(GH_TOK_BOR64):

			CASE(GH_TOK_BXOR8):
			CASE(GH_TOK_BXOR16):
			CASE(GH_TOK_BXOR32):
			CASE(GH_TOK_BXOR64):

			CASE(GH_TOK_LSHIFT8):
			CASE(GH_TOK_LSHIFT16):
			CASE(GH_TOK_LSHIFT32):
			CASE(GH_TOK_LSHIFT64):

			CASE(GH_TOK_RSHIFT8):
			CASE(GH_TOK_RSHIFT16):
			CASE(GH_TOK_RSHIFT32):
			CASE(GH_TOK_RSHIFT64):
			
			CASE(GH_OP_CALL_EQ):
			CASE(GH_OP_CALL_NEQ):
			CASE(GH_OP_CALL_GT):
			CASE(GH_OP_CALL_LT):

			CASE(GH_OP_JMP_EQ):
			CASE(GH_OP_JMP_NEQ):
			CASE(GH_OP_JMP_GT):
			CASE(GH_OP_JMP_LT):

			CASE(GH_OP_PUSH8):
			CASE(GH_OP_PUSH16):
			CASE(GH_OP_PUSH32):
			CASE(GH_OP_PUSH64):

			CASE(GH_OP_POP8):
			CASE(GH_OP_POP16):
			CASE(GH_OP_POP32):
			CASE(GH_OP_POP64):
		}
	}
}

void gh_vm_debug(FILE *fp, gh_vm *vm) {
	len ncode = 0;
	len ncpages = 0;
	len nfunctions = 0;
	if (vm->bc) {
		ncode = vm->bc->ncode;
		ncpages = vm->bc->ncpages;
		nfunctions = vm->bc->nfunctions;
	}

	(void) fprintf(stderr,
		"===== VM snapshot =====\n"
		"bc->ncode = " LEN_F "\n"
		"bc->ncpages = " LEN_F "\n"
		"bc->nfunctions = " LEN_F "\n"
		"registers:\n"
		"ip = " IDX_F "\n"
		"sp = " IDX_F "\n"
		"bp = " IDX_F "\n"
		"=====     END     =====\n",
		ncode,
		ncpages,
		nfunctions,
		vm->ip,
		vm->sp,
		vm->bp
	);
}

void gh_vm_deinit(gh_vm *vm) {
	free(vm->stack.b);
}
#endif
