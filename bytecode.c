#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>

#include "bytecode.h"
#include "token.h"
#include "log.h"
#include "vm.h"

static char *gh_slurp_src(char *file) {
	FILE *fp = fopen(file, "r");
	if (!fp) {
		gh_log(GH_LOG_WARN, "fopen: %s: %s", file, strerror(errno));
		return NULL;
	}
	(void) fseek(fp, 0, SEEK_END);
	long file_len = ftell(fp);
	if (file_len < 0) {
		gh_log(GH_LOG_WARN, "ftell: %s: %s", file, strerror(errno));
		goto e0;
	}

	char *buf = malloc(file_len + 1);
	if (!buf) {
		gh_log(GH_LOG_WARN, "alloc file buffer: %s", strerror(errno));
		goto e0;
	}

	(void) fseek(fp, 0, SEEK_SET);
	if (fread(buf, 1, (size_t) file_len, fp) != (size_t) file_len) {
		gh_log(GH_LOG_WARN, "fread: %s: %s", file, strerror(errno));
		goto e1;
	}
	buf[file_len] = 0;
	fclose(fp);
	return buf;

e1:
	free(buf);
e0:
	fclose(fp);
	return NULL;
}

void gh_bytecode_init(gh_bytecode *bytecode) {
	*bytecode = (gh_bytecode){};
}

typedef union {
	f32 f;
	u32 u;
} f32_u32;

typedef union {
	f64 f;
	u64 u;
} f64_u64;

typedef enum gh_token_id gh_type;

typedef struct {
	enum gh_local_id {
		GH_LOCAL_VAR,
		GH_LOCAL_FUN,
	} id;

	char *name;
	gh_type type;

	int emitted;
	// The offset from the base pointer
	// If this is positive, then it's an argument to the function
	// If it's negative, then it's a local variable
	i64 offset;
} gh_local;

typedef struct gh_local_list {
	gh_local *locals;
	u64 used;
	u64 size;

	struct gh_local_list *prev;
} gh_local_list;

static i64 offset_counter = 0;
static int compile_success = 0;
static jmp_buf compile_end;
#define COMPILE_FAIL() do { \
	gh_log(GH_LOG_ERR, "compilation failed from %s, line %d", __FUNCTION__, __LINE__); \
	longjmp(compile_end, 1); \
} while (0)

static const u64 local_bs = 8;
static gh_bytecode *bc;

static i64 gh_get_type_size(gh_type type) {
	switch (type) {
		case GH_TOK_KW_UNIT: return 0;
		case GH_TOK_KW_I8:
		case GH_TOK_KW_U8: return 1;
		case GH_TOK_KW_I16:
		case GH_TOK_KW_U16: return 2;
		case GH_TOK_KW_I32:
		case GH_TOK_KW_F32:
		case GH_TOK_KW_U32: return 4;
		case GH_TOK_KW_I64:
		case GH_TOK_KW_F64:
		case GH_TOK_KW_U64: return 8;
		default:
			gh_log(GH_LOG_ERR, "cannot get size of type: it's not a type");
			COMPILE_FAIL();
	}
}

static void *gh_emit_alloc(size_t s) {
	void *p = malloc(s);
	if (!p) {
		gh_log(GH_LOG_ERR, "malloc failed: %s", strerror(errno));
		COMPILE_FAIL();
	}
	return p;
}

static void *gh_emit_realloc(void *prev, size_t s) {
	void *n = realloc(prev, s);
	if (!n) {
		free(prev);
		gh_log(GH_LOG_ERR, "realloc failed: %s", strerror(errno));
		COMPILE_FAIL();
	}
	return n;
}

static void gh_init_local_list(gh_local_list *list, gh_local_list *prev) {
	list->locals = gh_emit_alloc(local_bs * sizeof(gh_local));
	list->used = 0;
	list->size = local_bs;
	list->prev = prev;
}

static i64 gh_calc_offset(i64 typesize) {
	offset_counter -= typesize;
	return offset_counter;
}

// Adds a local to the local list
// var x : u32 = 5
// ^ adds local
static gh_local *gh_add_local(gh_local_list *list, const gh_local *local) {
	if (list->used == list->size) {
		list->size += local_bs;
		list->locals = gh_emit_realloc(list->locals, list->size * sizeof(gh_local));
	}
	gh_local *new_local = &list->locals[list->used++];
	*new_local = *local;
	return new_local;
}

static gh_local *gh_find_local_depth1(gh_local_list *list, char *name) {
	for (u64 i = 0; i < list->used; i++) {
		if (strcmp(list->locals[i].name, name) == 0)
			return &list->locals[i];
	}
	return NULL;
}

// Finds a local in the local list (and previous lists)
// x += 5
// ^ finds local
static gh_local *gh_find_local(gh_local_list *list, char *name) {
	while (list) {
		gh_local *local = gh_find_local_depth1(list, name);
		if (local) return local;
		list = list->prev;
	}
	return NULL;
}

static gh_local *gh_get_req_local(gh_local_list *pl, gh_token *ident) {
	gh_local *local = gh_find_local(pl, ident->info.str);
	if (!local) {
		gh_log(GH_LOG_ERR, "undeclared identifier \"%s\"", ident->info.str);
		COMPILE_FAIL();
	}
	return local;
}

static const u64 emit_bs = 64;
static const u64 function_bs = 8;
static void gh_init_code(void) {
	bc->code.bytes = gh_emit_alloc(emit_bs);
	bc->code.nbytes = 0;
	bc->code.size = emit_bs;

	bc->fun.funs = gh_emit_alloc(function_bs);
	bc->fun.nfuns = 0;
	bc->fun.size = function_bs;
}

#define CHECK_EMIT(nb) do { \
	if (UNLIKELY((bc->code.size - bc->code.nbytes) < (nb))) { \
		bc->code.size += emit_bs; \
		bc->code.bytes = gh_emit_realloc(bc->code.bytes, bc->code.size); \
	} \
} while (0)

static void emitb(u8 b) {
	CHECK_EMIT(1);
	bc->code.bytes[bc->code.nbytes++] = b;
}

static void emitw(u16 w) {
	CHECK_EMIT(2);
	bc->code.bytes[bc->code.nbytes++] = (w >> 8) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (w >> 0) & 0xff;
}

static void emitdw(u32 dw) {
	CHECK_EMIT(4);
	bc->code.bytes[bc->code.nbytes++] = (dw >> 24) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (dw >> 16) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (dw >> 8) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (dw >> 0) & 0xff;
}

static void emitqw(u64 qw) {
	CHECK_EMIT(8);
	bc->code.bytes[bc->code.nbytes++] = (qw >> 56) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (qw >> 48) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (qw >> 40) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (qw >> 32) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (qw >> 24) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (qw >> 16) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (qw >> 8) & 0xff;
	bc->code.bytes[bc->code.nbytes++] = (qw >> 0) & 0xff;
}

static void gh_emit_expr(gh_local_list *pl, gh_ast *ast, gh_type *type);

#define BIT_CASE8  case GH_TOK_KW_I8:  case GH_TOK_KW_U8
#define BIT_CASE16 case GH_TOK_KW_I16: case GH_TOK_KW_U16
#define BIT_CASE32 case GH_TOK_KW_I32: case GH_TOK_KW_U32
#define BIT_CASE64 case GH_TOK_KW_I64: case GH_TOK_KW_U64
#define MULTI_CASE(inst) \
	BIT_CASE8: emitb((inst)); break; \
	BIT_CASE16: emitb((inst)+1); break; \
	case GH_TOK_KW_F32: \
	BIT_CASE32: emitb((inst)+2); break; \
	case GH_TOK_KW_F64: \
	BIT_CASE64: emitb((inst)+3); break;


// Emit code to cast the a register to another "type"
// TODO: floating point
static void gh_emit_cast(gh_type from, gh_type to) {
	switch (from) {
		BIT_CASE8: {
			switch (to) {
				BIT_CASE8: break;
				case GH_TOK_KW_I16:
					emitb(from == GH_TOK_KW_I8 ? GH_VM_SEXT_A8_16
						: GH_VM_ZEXT_A8_16);
					break;
				case GH_TOK_KW_U16: emitb(GH_VM_ZEXT_A8_16); break;
				case GH_TOK_KW_I32:
					gh_emit_cast(from, GH_TOK_KW_I16);
					gh_emit_cast(GH_TOK_KW_I16, GH_TOK_KW_I32);
					break;
				case GH_TOK_KW_U32:
					gh_emit_cast(from, GH_TOK_KW_U16);
					gh_emit_cast(GH_TOK_KW_U16, GH_TOK_KW_U32);
					break;
				case GH_TOK_KW_I64:
					gh_emit_cast(from, GH_TOK_KW_I32);
					gh_emit_cast(GH_TOK_KW_I32, GH_TOK_KW_I64);
					break;
				default: COMPILE_FAIL();
			}
			break;
		}
		BIT_CASE16: {
			switch (to) {
				BIT_CASE16: break;
				case GH_TOK_KW_I32:
					emitb(from == GH_TOK_KW_I16 ? GH_VM_SEXT_A16_32
						: GH_VM_ZEXT_A16_32);
					break;
				case GH_TOK_KW_U32: emitb(GH_VM_ZEXT_A16_32); break;
				case GH_TOK_KW_I64:
					gh_emit_cast(from, GH_TOK_KW_I32);
					gh_emit_cast(GH_TOK_KW_I32, GH_TOK_KW_I64);
					break;
				case GH_TOK_KW_U64:
					gh_emit_cast(from, GH_TOK_KW_U32);
					gh_emit_cast(GH_TOK_KW_U32, GH_TOK_KW_U64);
					break;
				default: COMPILE_FAIL();
			}
			break;
		}
		BIT_CASE32: {
			switch (to) {
				BIT_CASE32: break;
				case GH_TOK_KW_I64:
					emitb(from == GH_TOK_KW_I32 ? GH_VM_SEXT_A32_64
						: GH_VM_ZEXT_A32_64);
					break;
				case GH_TOK_KW_U64: emitb(GH_VM_ZEXT_A32_64); break;
				default: COMPILE_FAIL();
			}
			break;
		}
		BIT_CASE64: {
			switch (to) {
				BIT_CASE64: break;
				default: COMPILE_FAIL();
			}
			break;
		}
		default: COMPILE_FAIL();
	}
}

#define OP_MULTI(inst) switch (*type) { \
	MULTI_CASE(inst); \
	default: COMPILE_FAIL(); \
}

static void gh_emit_op_push(gh_type *type) {
	if (*type == GH_TOK_KW_F32 || *type == GH_TOK_KW_F64) COMPILE_FAIL(); // fow now
	OP_MULTI(GH_VM_PUSH8);
}
/*
static void gh_emit_op_pop(gh_type *type) {
	i64 typesize = -gh_get_type_size(*type);
	emitb(GH_VM_ADD_SP);
	emitqw((u64) typesize);
}
*/

static void gh_emit_primary(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	if (ast->primary.literal->id == GH_TOK_LIT_INT) {
		switch (*type) {
			case GH_TOK_KW_UNIT:
				*type = GH_TOK_KW_I32;
				goto do_i32;

			BIT_CASE8:
				emitb(GH_VM_MOV_IMM_A8);
				emitb((u8)ast->primary.literal->info.i);
				break;

			BIT_CASE16:
				emitb(GH_VM_MOV_IMM_A16);
				emitw((u16)ast->primary.literal->info.i);
				break;

			BIT_CASE32:
			do_i32:
				emitb(GH_VM_MOV_IMM_A32);
				emitdw((u32)ast->primary.literal->info.i);
				break;

			BIT_CASE64:
				emitb(GH_VM_MOV_IMM_A64);
				emitqw((u64)ast->primary.literal->info.i);
				break;

			case GH_TOK_KW_F32:
				emitb(GH_VM_MOV_IMM_A32);
				f32_u32 x;
				x.f = (f32) ast->primary.literal->info.i;
				emitdw(x.u);
				break;

			case GH_TOK_KW_F64:
				emitb(GH_VM_MOV_IMM_A64);
				f64_u64 y;
				y.f = ast->primary.literal->info.i;
				emitqw(y.u);
				break;

			default:
				COMPILE_FAIL();
		}
	} else if (ast->primary.literal->id == GH_TOK_LIT_FLOAT) {
		switch (*type) {
			case GH_TOK_KW_UNIT:
				*type = GH_TOK_KW_F32;
				goto do_f32;

			BIT_CASE8: BIT_CASE16: BIT_CASE32: BIT_CASE64:
				gh_log(GH_LOG_ERR, "can't cast a float literal to an int type");
				COMPILE_FAIL();

			case GH_TOK_KW_F32:
			do_f32:
				emitb(GH_VM_MOV_IMM_A32);
				f32_u32 x;
				x.f = (f32) ast->primary.literal->info.flt;
				emitdw(x.u);
				break;
			case GH_TOK_KW_F64:
				emitb(GH_VM_MOV_IMM_A64);
				f32_u32 y;
				y.f = ast->primary.literal->info.flt;
				emitqw(y.u);
				break;

			default:
				COMPILE_FAIL();
		}
	} else if (ast->primary.literal->id == GH_TOK_IDENT) {
		gh_local *local = gh_get_req_local(pl, ast->primary.literal);
		if (ast->primary.clist) {
			if (local->id != GH_LOCAL_FUN) {
				gh_log(GH_LOG_ERR, "can only call a function identifier");
				COMPILE_FAIL();
			}

			if (*type == GH_TOK_KW_UNIT)
				*type = local->type;

			gh_ast *clist = ast->primary.clist;
			i64 popsize = 0;
			i64 nc = 0;
			for (gh_ast *c = clist; c; c = c->clist.clist) {
				if (c->clist.expr) nc++;
			}

			gh_ast **clist_copy = NULL;
			if (nc) {
				clist_copy = gh_emit_alloc(nc * sizeof(gh_ast *));
				for (nc = 0; clist; nc++, clist = clist->clist.clist)
					clist_copy[nc] = clist->clist.expr;

				for (nc -= 1; nc >= 0; nc--) {
					if (clist_copy[nc]) {
						gh_type type = GH_TOK_KW_UNIT;
						gh_emit_expr(pl, clist_copy[nc], &type);
						gh_emit_op_push(&type);
						popsize += gh_get_type_size(type);
					}
				}
			}

			emitb(GH_VM_CALL);
			emitqw((u64) local->offset);
			if (popsize) {
				emitb(GH_VM_ADD_SP);
				emitqw((u64) popsize);
			}
			if (clist_copy)
				free(clist_copy);
		} else {
			if (local->id != GH_LOCAL_VAR) {
				gh_log(GH_LOG_ERR, "can only use a variable identifier in an expression");
				COMPILE_FAIL();
			}
			if (*type == GH_TOK_KW_UNIT)
				*type = local->type;

			switch (local->type) {
				MULTI_CASE(GH_VM_MOV_OFFSET_A8);
				default: COMPILE_FAIL();
			}
			emitqw((u64) local->offset);
			gh_emit_cast(local->type, *type);
		}
	} else {
		COMPILE_FAIL();
	}
}

static void gh_emit_unary(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_expr(pl, ast->unary.child, type);

	if (*type == GH_TOK_KW_UNIT) COMPILE_FAIL();
	if (ast->unary.op->id == GH_TOK_MINUS) {
		OP_MULTI(GH_VM_SIGN_A8);
	} else if (ast->unary.op->id == GH_TOK_NEG) {
		OP_MULTI(GH_VM_NEG_A8);
	} else if (ast->unary.op->id == GH_TOK_BNEG) {
		OP_MULTI(GH_VM_BNEG_A8);
	} else { COMPILE_FAIL(); }
}

static void gh_emit_branchop_prefix(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_expr(pl, ast->branch_op.first, type);
	gh_emit_op_push(type);
	gh_emit_expr(pl, ast->branch_op.second, type);
}

static void gh_emit_factor(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branchop_prefix(pl, ast, type);
	switch (ast->branch_op.op->id) {
		case GH_TOK_MULT: OP_MULTI(GH_VM_MUL8); break;
		case GH_TOK_DIV: OP_MULTI(GH_VM_DIV8); break;
		case GH_TOK_MODULO: fallthrough(); // todo
		default: COMPILE_FAIL();
	}
}

static void gh_emit_adder(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branchop_prefix(pl, ast, type);
	switch (ast->branch_op.op->id) {
		case GH_TOK_PLUS: OP_MULTI(GH_VM_ADD8); break;
		case GH_TOK_MINUS: OP_MULTI(GH_VM_SUB8); break;
		default: COMPILE_FAIL();
	}
}

static void gh_emit_shifter(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branchop_prefix(pl, ast, type);
	switch (ast->branch_op.op->id) {
		case GH_TOK_LSHIFT: OP_MULTI(GH_VM_LSHIFT8); break;
		case GH_TOK_RSHIFT: OP_MULTI(GH_VM_RSHIFT8); break;
		default: COMPILE_FAIL();
	}
}

static void gh_emit_relation(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branchop_prefix(pl, ast, type);
	OP_MULTI(GH_VM_CMP8);
	switch (ast->branch_op.op->id) {
		case GH_TOK_GT: emitb(GH_VM_SETGT); break;
		case GH_TOK_LT: emitb(GH_VM_SETLT); break;
		case GH_TOK_GEQ: emitb(GH_VM_SETGE); break;
		case GH_TOK_LEQ: emitb(GH_VM_SETLE); break;
		default: COMPILE_FAIL();
	}
}

static void gh_emit_compare(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branchop_prefix(pl, ast, type);
	OP_MULTI(GH_VM_CMP8);
	switch (ast->branch_op.op->id) {
		case GH_TOK_EQ: emitb(GH_VM_SETEQ); break;
		case GH_TOK_NEQ: emitb(GH_VM_SETNEQ); break;
		default: COMPILE_FAIL();
	}
}

static void gh_emit_branch_prefix(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_expr(pl, ast->branch.first, type);
	gh_emit_op_push(type);
	gh_emit_expr(pl, ast->branch.second, type);
}

static void gh_emit_band(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branch_prefix(pl, ast, type);
	OP_MULTI(GH_VM_BAND8);
}

static void gh_emit_bxor(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branch_prefix(pl, ast, type);
	OP_MULTI(GH_VM_BXOR8);
}

static void gh_emit_bor(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branch_prefix(pl, ast, type);
	OP_MULTI(GH_VM_BOR8);
}

static void gh_emit_and(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branch_prefix(pl, ast, type);
	OP_MULTI(GH_VM_AND8);
}

static void gh_emit_or(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_emit_branch_prefix(pl, ast, type);
	OP_MULTI(GH_VM_OR8);
}

static void gh_emit_assgn(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	gh_local *local = gh_get_req_local(pl, ast->assgn.ident);
	if (*type == GH_TOK_KW_UNIT)
		*type = local->type;
	if (ast->assgn.op->id == GH_TOK_ASSIGN) {
		gh_emit_expr(pl, ast->assgn.expr, type);
		OP_MULTI(GH_VM_MOV_A_OFFSET8);
		emitqw((u64) local->offset);
		return ;
	}

	OP_MULTI(GH_VM_MOV_OFFSET_A8);
	emitqw((u64) local->offset);
	gh_emit_op_push(type);
	gh_emit_expr(pl, ast->assgn.expr, type);
	switch (ast->assgn.op->id) {
		case GH_TOK_PLUS_ASSIGN: OP_MULTI(GH_VM_ADD8); break;
		case GH_TOK_MINUS_ASSIGN: OP_MULTI(GH_VM_SUB8); break;
		case GH_TOK_MULT_ASSIGN: OP_MULTI(GH_VM_MUL8); break;
		case GH_TOK_DIV_ASSIGN: OP_MULTI(GH_VM_DIV8); break;
		case GH_TOK_MODULO_ASSIGN: fallthrough(); // todo
		default: COMPILE_FAIL();
	}
	OP_MULTI(GH_VM_MOV_A_OFFSET8);
	emitqw((u64) local->offset);
}

// Emits code to evaluate an expression, the result stored in register a.
static void gh_emit_expr(gh_local_list *pl, gh_ast *ast, gh_type *type) {
	switch (ast->type) {
		case GH_AST_ASSGN:    gh_emit_assgn(pl, ast, type);    break;
		case GH_AST_OR:       gh_emit_or(pl, ast, type);       break;
		case GH_AST_AND:      gh_emit_and(pl, ast, type);      break;
		case GH_AST_BOR:      gh_emit_bor(pl, ast, type);      break;
		case GH_AST_BXOR:     gh_emit_bxor(pl, ast, type);     break;
		case GH_AST_BAND:     gh_emit_band(pl, ast, type);     break;
		case GH_AST_COMPARE:  gh_emit_compare(pl, ast, type);  break;
		case GH_AST_RELATION: gh_emit_relation(pl, ast, type); break;
		case GH_AST_SHIFTER:  gh_emit_shifter(pl, ast, type);  break;
		case GH_AST_ADDER:    gh_emit_adder(pl, ast, type);    break;
		case GH_AST_FACTOR:   gh_emit_factor(pl, ast, type);   break;
		case GH_AST_UNARY:    gh_emit_unary(pl, ast, type);    break;
		case GH_AST_PRIMARY:  gh_emit_primary(pl, ast, type);  break;
		default: COMPILE_FAIL();
	}
}

// Emits code to create a variable on the stack.
// var statements specify the name of the variable, the type of the variable,
// and optionally the expression it's initialized with. If there's no expression
// provided, it's zero initialized.
//
// Specifically, this places the variable on the stack, below the base pointer.
// Function parameters are stored above the base pointer, and local variables are
// stored below it:
//
// fun example(i32 param) -> unit begin
//     i32 local;
// end
//
// |
// V
//
// v- sp       v- bp
// v           v
// |  4 bytes  |  8 bytes  |  4 bytes  |
// ^           ^           ^- i32 param
// ^           ^- base pointer
// ^- i32 local
//
// The base pointer is on the stack because it was pushed there to create a new
// stack frame. The calling convention is different, but in x86_64 asm it would
// look something like this (not verified):
//
// push rbp      ; enter
// mov rbp, rsp
//
// mov eax, DWORD PTR [rbp+8]  ; i32 param
// mov ebx, DWORD PTR [rbp-4]  ; i32 local
//
// ...
//
// mov rsp, rbp  ; leave
// pop rbp
//
static void gh_emit_var(gh_local_list *pl, gh_ast *ast) {
	gh_type type = ast->var.type->id;

	if (ast->var.expr) {
		gh_emit_expr(pl, ast->var.expr, &type);
	} else {
		emitb(GH_VM_MOV_IMM_A64);
		emitqw((u64) 0);
	}

	i64 typesize = gh_get_type_size(type);
	i64 offset = gh_calc_offset(typesize);
	if (!typesize) {
		gh_log(GH_LOG_ERR, "cannot declare a variable of type unit");
		COMPILE_FAIL();
	}

	switch (typesize) {
		case 1: emitb(GH_VM_MOV_A_OFFSET8); break;
		case 2: emitb(GH_VM_MOV_A_OFFSET16); break;
		case 4: emitb(GH_VM_MOV_A_OFFSET32); break;
		case 8: emitb(GH_VM_MOV_A_OFFSET64); break;
		default: COMPILE_FAIL(); break;
	}
	emitqw((u64) offset);
	gh_add_local(pl, &(const gh_local) {
		.id = GH_LOCAL_VAR,
		.name = ast->var.ident->info.str,
		.type = type,
		.offset = offset,
	});
}

static void gh_emit_statement(gh_local_list *pl, gh_ast *ast);

static void gh_emit_block(gh_local_list *pl, gh_ast *ast) {
	gh_local_list nlist;
	gh_init_local_list(&nlist, pl);
	gh_emit_statement(&nlist, ast);
}

static void gh_emit_if(gh_local_list *pl, gh_ast *ast) {
	gh_type _type = GH_TOK_KW_UNIT;
	gh_type *type = &_type;
	gh_emit_expr(pl, ast->ifexpr.expr, type);
	OP_MULTI(GH_VM_JZ8);
	u64 iszero = bc->code.nbytes;
	emitqw(0);

	gh_emit_block(pl, ast->ifexpr.statement);
	emitb(GH_VM_JMP);
	u64 jmp_cont = bc->code.nbytes;
	emitqw(0);

	u64 zero_addr = bc->code.nbytes;
	if (ast->ifexpr.endif)
		gh_emit_block(pl, ast->ifexpr.endif);

	u64 end = bc->code.nbytes;
	bc->code.nbytes = iszero;
	emitqw(zero_addr);
	bc->code.nbytes = jmp_cont;
	emitqw(end);
	bc->code.nbytes = end;
}

static void gh_emit_while(gh_local_list *pl, gh_ast *ast) {
	gh_type _type = GH_TOK_KW_UNIT;
	gh_type *type = &_type;
	u64 top = bc->code.nbytes;
	gh_emit_expr(pl, ast->whileexpr.expr, type);
	OP_MULTI(GH_VM_JZ8);
	u64 iszero = bc->code.nbytes;
	emitqw(0);

	gh_emit_block(pl, ast->whileexpr.block);
	emitb(GH_VM_JMP);
	emitqw(top);

	u64 end = bc->code.nbytes;
	bc->code.nbytes = iszero;
	emitqw(end);
	bc->code.nbytes = end;
}

static gh_type function_return_type;
static void gh_emit_return(gh_local_list *pl, gh_ast *ast) {
	if (ast->returnexpr.expr) {
		gh_type type = GH_TOK_KW_UNIT;
		gh_emit_expr(pl, ast->returnexpr.expr, &type);
		if (function_return_type == GH_TOK_KW_UNIT) {
			gh_log(GH_LOG_ERR, "returning an expression in a function returning unit");
			COMPILE_FAIL();
		} else {
			gh_emit_cast(type, function_return_type);
		}
	}
	emitb(GH_VM_LEAVE);
	emitb(GH_VM_RET);
}

static void gh_emit_statement(gh_local_list *pl, gh_ast *ast) {
	while (ast) {
		gh_ast *child = ast->statement.child;
		switch (child->type) {
			case GH_AST_VAR: gh_emit_var(pl, child); break;
			case GH_AST_IF:  gh_emit_if(pl, child); break;
			case GH_AST_MATCH: break;
			case GH_AST_WHILE: gh_emit_while(pl, child); break;
			case GH_AST_RETURN: gh_emit_return(pl, child); break;
			case GH_AST_STATEMENT: gh_emit_block(pl, child); break;
			default: gh_emit_expr(pl, child, &(gh_type){GH_TOK_KW_UNIT}); break;
		}
		ast = ast->statement.statement;
	}
}

static void gh_emit_fun(gh_local_list *pl, gh_ast *ast) {
	gh_local *found;
	if ((found = gh_find_local(pl, ast->fun.ident->info.str))) {
		if (found->id != GH_LOCAL_FUN) {
			gh_log(GH_LOG_ERR, "function declaration of already declared variable");
			COMPILE_FAIL();
		}
		if (found->emitted) {
			gh_log(GH_LOG_ERR, "redeclaration of function");
			COMPILE_FAIL();
		}
	}
	gh_add_local(pl, &(const gh_local) {
		.id = GH_LOCAL_FUN,
		.name = ast->fun.ident->info.str,
		.type = ast->fun.type->id,
		.emitted = 1,
	});
	function_return_type = ast->fun.type->id;

	gh_local_list fun_list;
	gh_init_local_list(&fun_list, pl);
	gh_ast *flist = ast->fun.flist;
	i64 offset = 8; // 8 bytes to skip over base pointer
	// | bp | i32 | arg1 | ... ]
	// ^    ^     ^
	// ^    ^     ^- bp+12
	// ^    ^- bp+8
	// ^- where bp points to (it's also stored there on the stack).
	while (flist) {
		gh_add_local(&fun_list, &(const gh_local) {
			.id = GH_LOCAL_VAR,
			.name = flist->flist.ident->info.str,
			.type = flist->flist.type->id,
			.offset = offset,
		});
		int typesize = gh_get_type_size(flist->flist.type->id);
		if (!typesize) {
			gh_log(GH_LOG_ERR, "cannot have unit type in function parameters");
			COMPILE_FAIL();
		}
		offset += typesize;
		flist = flist->flist.flist; // wow I'm so good at naming things
	}

	if (bc->fun.nfuns == bc->fun.size) {
		bc->fun.size += function_bs;
		bc->fun.funs = gh_emit_realloc(bc->fun.funs, bc->fun.size);
	}

	gh_function *fun = &bc->fun.funs[bc->fun.nfuns++];
	fun->offset = bc->code.nbytes;

	emitb(GH_VM_ENTER);
	emitb(GH_VM_ADD_SP);

	u64 old_nbytes = bc->code.nbytes;
	emitqw(0);

	gh_emit_statement(&fun_list, ast->fun.block);

	// This may be a duplicate for functions that return something at the end,
	// so that may want to be optimized later
	emitb(GH_VM_MOV_IMM_A64);
	emitqw(0);
	emitb(GH_VM_LEAVE);
	emitb(GH_VM_RET);

	u64 tmp = bc->code.nbytes;
	bc->code.nbytes = old_nbytes;
	emitqw((u64) offset_counter);
	bc->code.nbytes = tmp;

	fun->nbytes = bc->code.nbytes - fun->offset;
	offset_counter = 0;
}

static void gh_bytecode_compile(gh_bytecode *bytecode, gh_ast *ast) {
	if (setjmp(compile_end))
		return ;

	bc = bytecode;
	gh_init_code();

	gh_local_list list;
	gh_init_local_list(&list, NULL);
	gh_ast *decl = ast->prgm.decl;
	while (decl) {
		gh_ast *child = decl->decl.child;
		switch (child->type) {
			case GH_AST_FUN: gh_emit_fun(&list, child); break;
			//case GH_AST_VAR: gh_emit_global(&list, child); break;
			default: COMPILE_FAIL();
		}
		decl = decl->decl.decl;
	}

	compile_success = 1;
}

// for testing purposes
void gh_bytecode_mem(gh_bytecode *bytecode, gh_ast *ast) {
	gh_bytecode_compile(bytecode, ast);
	if (!compile_success)
		gh_log(GH_LOG_ERR, "failed to compile ast");
}

int gh_bytecode_src(gh_bytecode *bytecode, char *file) {
	char *src = gh_slurp_src(file);
	if (!src)
		return -1;
	gh_token *tokens = gh_token_init(src);
	free(src);
	if (tokens) {
		gh_ast *ast;
		if ((ast = gh_ast_init(tokens))) {
			gh_bytecode_compile(bytecode, ast);
			gh_ast_deinit(ast);
		}
		gh_token_deinit(tokens);
	}
	if (!compile_success) {
		gh_log(GH_LOG_ERR, "failed to compile %s", file);
		return -1;
	}

	return 0;
}

void gh_bytecode_deinit(gh_bytecode *bytecode) {
	(void) bytecode;
}
