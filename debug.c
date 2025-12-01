#include "debug.h"
#include "log.h"
#include <inttypes.h>

static char *token_map[] = {
	[GH_TOK_KW_UNIT] = "TOK_KW_UNIT",
	[GH_TOK_KW_I8] = "TOK_KW_I8",
	[GH_TOK_KW_U8] = "TOK_KW_U8",
	[GH_TOK_KW_I16] = "TOK_KW_I16",
	[GH_TOK_KW_U16] = "TOK_KW_U16",
	[GH_TOK_KW_I32] = "TOK_KW_I32",
	[GH_TOK_KW_U32] = "TOK_KW_U32",
	[GH_TOK_KW_I64] = "TOK_KW_I64",
	[GH_TOK_KW_U64] = "TOK_KW_U64",
	[GH_TOK_KW_F32] = "TOK_KW_F32",
	[GH_TOK_KW_F64] = "TOK_KW_F64",

	[GH_TOK_KW_VAR] = "TOK_KW_VAR",
	[GH_TOK_KW_FUN] = "TOK_KW_FUN",

	[GH_TOK_KW_WHILE] = "TOK_KW_WHILE",
	[GH_TOK_KW_IF] = "TOK_KW_IF",
	[GH_TOK_KW_THEN] = "TOK_KW_THEN",
	[GH_TOK_KW_ELSE] = "TOK_KW_ELSE",
	[GH_TOK_KW_MATCH] = "TOK_KW_MATCH",

	[GH_TOK_KW_BEGIN] = "TOK_KW_BEGIN",
	[GH_TOK_KW_END] = "TOK_KW_END",

	[GH_TOK_KW_TRUE] = "TOK_KW_TRUE",
	[GH_TOK_KW_FALSE] = "TOK_KW_FALSE",

	[GH_TOK_IDENT] = "TOK_IDENT",
	[GH_TOK_LIT_INT] = "TOK_LIT_INT",
	[GH_TOK_LIT_FLOAT] = "TOK_LIT_FLOAT",
	[GH_TOK_LIT_STRING] = "TOK_LIT_STRING",

	[GH_TOK_RARROW] = "TOK_RARROW",
	[GH_TOK_COLON] = "TOK_COLON",

	[GH_TOK_ASSIGN] = "TOK_ASSIGN",
	[GH_TOK_PLUS] = "TOK_PLUS",
	[GH_TOK_MINUS] = "TOK_MINUS",
	[GH_TOK_MULT] = "TOK_MULT",
	[GH_TOK_DIV] = "TOK_DIV",
	[GH_TOK_MODULO] = "TOK_MODULO",
	[GH_TOK_PLUS_ASSIGN] = "TOK_PLUS_ASSIGN",
	[GH_TOK_MINUS_ASSIGN] = "TOK_MINUS_ASSIGN",
	[GH_TOK_MULT_ASSIGN] = "TOK_MULT_ASSIGN",
	[GH_TOK_DIV_ASSIGN] = "TOK_DIV_ASSIGN",
	[GH_TOK_MODULO_ASSIGN] = "TOK_MODULO_ASSIGN",

	[GH_TOK_EQ] = "TOK_EQ",
	[GH_TOK_NEQ] = "TOK_NEQ",
	[GH_TOK_GT] = "TOK_GT",
	[GH_TOK_LT] = "TOK_LT",
	[GH_TOK_GEQ] = "TOK_GEQ",
	[GH_TOK_LEQ] = "TOK_LEQ",

	[GH_TOK_NEG] = "TOK_NEG",
	[GH_TOK_AND] = "TOK_AND",
	[GH_TOK_OR] = "TOK_OR",

	[GH_TOK_BNEG] = "TOK_BNEG",
	[GH_TOK_BAND] = "TOK_BAND",
	[GH_TOK_BOR] = "TOK_BOR",
	[GH_TOK_BXOR] = "TOK_BXOR",
	[GH_TOK_LSHIFT] = "TOK_LSHIFT",
	[GH_TOK_RSHIFT] = "TOK_RSHIFT",
	
	[GH_TOK_LPAREN] = "TOK_LPAREN",
	[GH_TOK_RPAREN] = "TOK_RPAREN",

	[GH_TOK_COMMA] = "TOK_COMMA",
	[GH_TOK_EOF]   = "EOF",
};

static const char *ast_map[] = {
	[GH_AST_PRGM]="GH_AST_PRGM",      [GH_AST_DECL]="GH_AST_DECL",
	[GH_AST_FUN]="GH_AST_FUN",       [GH_AST_FLIST]="GH_AST_FLIST",
	[GH_AST_VAR]="GH_AST_VAR",
	[GH_AST_STATEMENT]="GH_AST_STATEMENT", [GH_AST_CLIST]="GH_AST_CLIST",
	[GH_AST_IF]="GH_AST_IF",        [GH_AST_MATCH]="GH_AST_MATCH",
	[GH_AST_MSELECT]="GH_AST_MSELECT",   [GH_AST_WHILE]="GH_AST_WHILE",
	[GH_AST_ASSGN]="GH_AST_ASSGN",     [GH_AST_RETURN]="GH_AST_RETURN",
	[GH_AST_OR]="GH_AST_OR",        [GH_AST_AND]="GH_AST_AND",
	[GH_AST_BOR]="GH_AST_BOR",       [GH_AST_BXOR]="GH_AST_BXOR",
	[GH_AST_BAND]="GH_AST_BAND",      [GH_AST_COMPARE]="GH_AST_COMPARE",
	[GH_AST_RELATION]="GH_AST_RELATION",  [GH_AST_SHIFTER]="GH_AST_SHIFTER",
	[GH_AST_ADDER]="GH_AST_ADDER",     [GH_AST_FACTOR]="GH_AST_FACTOR",
	[GH_AST_UNARY]="GH_AST_UNARY",     [GH_AST_PRIMARY]="GH_AST_PRIMARY",
};


void gh_token_print(FILE *fp, gh_token *token) {
	if (!token) {
		(void) fprintf(fp, "(null tok)");
		return ;
	}
	(void) fprintf(fp, "%s", token_map[token->id]);
	switch (token->id) {
		case GH_TOK_IDENT:
		case GH_TOK_LIT_STRING:
			(void) fprintf(stderr, "(\"%s\")", token->info.str);
			break;
		case GH_TOK_LIT_INT:
			(void) fprintf(stderr, "(%" PRIu64 ")", token->info.i);
			break;
		case GH_TOK_LIT_FLOAT:
			(void) fprintf(stderr, "(%lf)", token->info.flt);
			break;
		default: break;
	}
}

void gh_token_debug(FILE *fp, gh_token *tokens) {
	do {
		gh_token_print(fp, tokens);
		(void) fputc(' ', fp);
	} while ((tokens++)->id != GH_TOK_EOF);
	(void) fputc('\n', fp);
}

void gh_token_debug_len(FILE *fp, gh_token *tokens, u64 ntokens) {
	for (u64 i = 0; i < ntokens; i++) {
		gh_token_print(fp, &tokens[i]);
		(void) fputc(' ', fp);
	}
	(void) fprintf(fp, "%s", token_map[GH_TOK_EOF]);
	(void) fputc('\n', fp);
}

static void gh_ast_debug_rec(gh_ast *root, int level) {
	#define PAD() for (int i = 0; i < level; i++) (void) fprintf(stderr, "│ ");
	#define ARR() (void) fprintf(stderr, "├─")
	#define DO(child) gh_ast_debug_rec((child), level+1)
	#define DOTOK(child) gh_token_print(stderr, (child)); (void) fputc('\n', stderr)

	if (!root) {
		(void) fprintf(stderr, "(null)\n");
		return ;
	}

	(void) fprintf(stderr, "(%s)\n", ast_map[root->type]);
	PAD(); ARR();
	switch (root->type) {
		case GH_AST_PRGM: gh_ast_debug_rec(root->prgm.decl, level+1); break;
		case GH_AST_DECL:
			DO(root->decl.child); PAD(); ARR();
			DO(root->decl.decl); break;
		case GH_AST_FUN:
			DOTOK(root->fun.ident); PAD(); ARR();
			DO(root->fun.flist); PAD(); ARR();
			DOTOK(root->fun.type); PAD(); ARR();
			DO(root->fun.block); break;
		case GH_AST_FLIST:
			DOTOK(root->flist.type); PAD(); ARR();
			DOTOK(root->flist.ident); PAD(); ARR();
			DO(root->flist.flist); break;
		case GH_AST_VAR:
			DOTOK(root->var.ident); PAD(); ARR();
			DOTOK(root->var.type); PAD(); ARR();
			DO(root->var.expr); break;
		case GH_AST_STATEMENT:
			DO(root->statement.child); PAD(); ARR();
			DO(root->statement.statement); break;
		case GH_AST_IF:
			DO(root->ifexpr.expr); PAD(); ARR();
			DO(root->ifexpr.statement); PAD(); ARR();
			DO(root->ifexpr.endif); break;
		case GH_AST_MATCH:
			DO(root->match.expr); PAD(); ARR();
			DO(root->match.mselect); break;
		case GH_AST_MSELECT:
			DO(root->mselect.expr); PAD(); ARR();
			DO(root->mselect.statement); PAD(); ARR();
			DO(root->mselect.mselect); break;
		case GH_AST_WHILE:
			DO(root->whileexpr.expr); PAD(); ARR();
			DO(root->whileexpr.block); break;
		case GH_AST_RETURN:
			DO(root->whileexpr.expr); break;
		case GH_AST_OR: case GH_AST_AND:
		case GH_AST_BOR: case GH_AST_BXOR:
		case GH_AST_BAND:
			DO(root->branch.first); PAD(); ARR();
			DO(root->branch.second); break;
		case GH_AST_COMPARE: case GH_AST_RELATION:
		case GH_AST_SHIFTER: case GH_AST_ADDER:
		case GH_AST_FACTOR:
			DOTOK(root->branch_op.op); PAD(); ARR();
			DO(root->branch_op.first); PAD(); ARR();
			DO(root->branch_op.second); break;
		case GH_AST_ASSGN:
			DOTOK(root->assgn.ident); PAD(); ARR();
			DOTOK(root->assgn.op); PAD(); ARR();
			DO(root->assgn.expr); break;
		case GH_AST_UNARY:
			DOTOK(root->unary.op); PAD(); ARR();
			DO(root->unary.child); break;
		case GH_AST_PRIMARY:
			DOTOK(root->primary.literal); PAD(); ARR();
			DO(root->primary.clist); break;
		case GH_AST_CLIST:
			DO(root->clist.expr); PAD(); ARR();
			DO(root->clist.clist); break;
		default: break;
	}
}

void gh_ast_debug(gh_ast *root) {
	gh_ast_debug_rec(root, 0);
}

static char *op_map[] = {
	[GH_VM_MOV_A_OFFSET8] = "mov.b",
	[GH_VM_MOV_A_OFFSET16] = "mov.w",
	[GH_VM_MOV_A_OFFSET32] = "mov.dw",
	[GH_VM_MOV_A_OFFSET64] = "mov.qw",

	[GH_VM_MOV_OFFSET_A8] = "mov.b",
	[GH_VM_MOV_OFFSET_A16] = "mov.w",
	[GH_VM_MOV_OFFSET_A32] = "mov.dw",
	[GH_VM_MOV_OFFSET_A64] = "mov.qw",

	[GH_VM_SIGN_A8] = "sign.b",
	[GH_VM_SIGN_A16] = "sign.w",
	[GH_VM_SIGN_A32] = "sign.dw",
	[GH_VM_SIGN_A64] = "sign.qw",

	[GH_VM_ZEXT_A8_16] = "zext.b.w",
	[GH_VM_ZEXT_A16_32] = "zext.w.dw",
	[GH_VM_ZEXT_A32_64] = "zext.dw.qw",

	[GH_VM_SEXT_A8_16] = "sext.b.w",
	[GH_VM_SEXT_A16_32] = "sext.w.dw",
	[GH_VM_SEXT_A32_64] = "sext.dw.qw",

	[GH_VM_MOV_IMM_A8] = "mov.b",
	[GH_VM_MOV_IMM_A16] = "mov.w",
	[GH_VM_MOV_IMM_A32] = "mov.dw",
	[GH_VM_MOV_IMM_A64] = "mov.qw",

	[GH_VM_NEG_A8] = "neg.b",
	[GH_VM_NEG_A16] = "neg.w",
	[GH_VM_NEG_A32] = "neg.dw",
	[GH_VM_NEG_A64] = "neg.qw",

	[GH_VM_BNEG_A8] = "bneg.b",
	[GH_VM_BNEG_A16] = "bneg.w",
	[GH_VM_BNEG_A32] = "bneg.dw",
	[GH_VM_BNEG_A64] = "bneg.qw",

	[GH_VM_ENTER] = "enter",
	[GH_VM_LEAVE] = "leave",

	[GH_VM_ADD_SP] = "add.sp",

	[GH_VM_PUSH8] = "push.b",
	[GH_VM_PUSH16] = "push.w",
	[GH_VM_PUSH32] = "push.dw",
	[GH_VM_PUSH64] = "push.qw",

	[GH_VM_ADD8] = "add.b",
	[GH_VM_ADD16] = "add.w",
	[GH_VM_ADD32] = "add.dw",
	[GH_VM_ADD64] = "add.qw",

	[GH_VM_SUB8] = "sub.b",
	[GH_VM_SUB16] = "sub.w",
	[GH_VM_SUB32] = "sub.dw",
	[GH_VM_SUB64] = "sub.qw",

	[GH_VM_MUL8] = "mul.b",
	[GH_VM_MUL16] = "mul.w",
	[GH_VM_MUL32] = "mul.dw",
	[GH_VM_MUL64] = "mul.qw",

	[GH_VM_DIV8] = "div.b",
	[GH_VM_DIV16] = "div.w",
	[GH_VM_DIV32] = "div.dw",
	[GH_VM_DIV64] = "div.qw",

	[GH_VM_LSHIFT8] = "lshift.b",
	[GH_VM_LSHIFT16] = "lshift.w",
	[GH_VM_LSHIFT32] = "lshift.dw",
	[GH_VM_LSHIFT64] = "lshift.qw",

	[GH_VM_RSHIFT8] = "rshift.b",
	[GH_VM_RSHIFT16] = "rshift.w",
	[GH_VM_RSHIFT32] = "rshift.dw",
	[GH_VM_RSHIFT64] = "rshift.qw",

	[GH_VM_CMP8] = "cmp.b",
	[GH_VM_CMP16] = "cmp.w",
	[GH_VM_CMP32] = "cmp.dw",
	[GH_VM_CMP64] = "cmp.qw",

	[GH_VM_SETLT] = "setlt",
	[GH_VM_SETGT] = "setgt",
	[GH_VM_SETLE] = "setle",
	[GH_VM_SETGE] = "setge",
	[GH_VM_SETEQ] = "seteq",
	[GH_VM_SETNEQ] = "setneq",

	[GH_VM_BAND8] = "band.b",
	[GH_VM_BAND16] = "band.w",
	[GH_VM_BAND32] = "band.dw",
	[GH_VM_BAND64] = "band.qw",

	[GH_VM_BXOR8] = "bxor.b",
	[GH_VM_BXOR16] = "bxor.w",
	[GH_VM_BXOR32] = "bxor.dw",
	[GH_VM_BXOR64] = "bxor.qw",

	[GH_VM_BOR8] = "bor.b",
	[GH_VM_BOR16] = "bor.w",
	[GH_VM_BOR32] = "bor.dw",
	[GH_VM_BOR64] = "bor.qw",

	[GH_VM_AND8] = "and.b",
	[GH_VM_AND16] = "and.w",
	[GH_VM_AND32] = "and.dw",
	[GH_VM_AND64] = "and.qw",

	[GH_VM_OR8] = "or.b",
	[GH_VM_OR16] = "or.w",
	[GH_VM_OR32] = "or.dw",
	[GH_VM_OR64] = "or.qw",

	[GH_VM_JZ8] = "jz.b",
	[GH_VM_JZ16] = "jz.w",
	[GH_VM_JZ32] = "jz.dw",
	[GH_VM_JZ64] = "jz.qw",
	[GH_VM_JMP] = "jmp",
	[GH_VM_RET] = "ret",
};
static const gh_vm_op last_implemented = GH_VM_RET;

#define CHECK_DISAS(b, e, nb) do { \
	if ((e-b)<(nb)) { \
		(void) fprintf(fp, "\n... end of bytecode\n"); \
		return -1; \
	} \
} while (0)

static int gh_disas_imm8(FILE *fp, u8 *b, u8 *e) {
	CHECK_DISAS(b, e, 1);
	(void) fprintf(fp, "%" PRIu8, *b);
	return 1;
}

static int gh_disas_imm16(FILE *fp, u8 *b, u8 *e) {
	CHECK_DISAS(b, e, 2);
	u16 u = ((u16)b[0] << 8) | ((u16)b[1] << 0);
	(void) fprintf(fp, "%" PRIu16, u);
	return 2;
}

static int gh_disas_imm32(FILE *fp, u8 *b, u8 *e) {
	CHECK_DISAS(b, e, 4);
	u32 u = ((u32)b[0] << 24)
			| ((u32) b[1] << 16)
			| ((u32) b[2] << 8)
			| ((u32) b[3] << 0);
	(void) fprintf(fp, "%" PRIu32, u);
	return 4;
}

static u64 gh_disas_get64(FILE *fp, u8 *b, u8 *e) {
	CHECK_DISAS(b, e, 8);
	return
		((u64) b[0] << 56)
		| ((u64) b[1] << 48)
		| ((u64) b[2] << 40)
		| ((u64) b[3] << 32)
		| ((u64) b[4] << 24)
		| ((u64) b[5] << 16)
		| ((u64) b[6] << 8)
		| ((u64) b[7] << 0);
}

static int gh_disas_imm64(FILE *fp, u8 *b, u8 *e) {
	CHECK_DISAS(b, e, 8);
	u64 u = gh_disas_get64(fp, b, e);
	(void) fprintf(stderr, "%" PRIu64, u);
	return 8;
}

static int gh_disas_offset(FILE *fp, u8 *b, u8 *e) {
	CHECK_DISAS(b, e, 8);
	i64 of = (i64) gh_disas_get64(fp, b, e);
	i64 of_abs = of < 0 ? -of : of;
	(void) fprintf(fp, "%c%" PRIi64, of<0 ? '-' : '+', of_abs);
	return 8;
}

static int gh_disas_addr(FILE *fp, u8 *b, u8 *e) {
	CHECK_DISAS(b, e, 8);
	u64 addr = gh_disas_get64(fp, b, e);
	(void) fprintf(fp, "0x%llx", addr);
	return 8;
}

static void gh_disas_func(FILE *fp, gh_bytecode *bc, gh_function *fun) {
	(void) fprintf(fp, "\n=== New function ===\n");
	u8 *b = bc->code.bytes + fun->offset;
	u8 *e = b + fun->nbytes;

	int c;
	for (; b < e; b += c) {
		c = 0;

		(void) fprintf(fp, "0x%lx\t", b - bc->code.bytes);

		if (*b > last_implemented)
			(void) fprintf(fp, "unimplemented ");
		else
			(void) fprintf(fp, "%s ", op_map[*b]);

		#define A_OFFSET(inst) \
		case (inst+0): case (inst+1): case (inst+2): case (inst+3): \
			(void) fprintf(fp, "a, [bp"); \
			c = gh_disas_offset(fp, b, e); \
			if (c < 0) goto end; \
			(void) fprintf(fp, "]"); \
			break;

		#define OFFSET_A(inst) \
		case (inst+0): case (inst+1): case (inst+2): case (inst+3): \
			(void) fprintf(fp, "[bp"); \
			c = gh_disas_offset(fp, b, e); \
			if (c < 0) goto end; \
			(void) fprintf(fp, "], a"); \
			break;

		#define SINGLE_A(inst) \
		case (inst+0): case (inst+1): case (inst+2): case (inst+3): \
			(void) fprintf(fp, "a"); break;

		switch (*b++) {
			A_OFFSET(GH_VM_MOV_A_OFFSET8);
			OFFSET_A(GH_VM_MOV_OFFSET_A8);
			SINGLE_A(GH_VM_SIGN_A8);

			case GH_VM_ZEXT_A8_16:
			case GH_VM_ZEXT_A16_32:
			case GH_VM_ZEXT_A32_64:
				(void) fprintf(fp, "a");
				break;

			case GH_VM_SEXT_A8_16:
			case GH_VM_SEXT_A16_32:
			case GH_VM_SEXT_A32_64:
				(void) fprintf(fp, "a");
				break;

			case GH_VM_MOV_IMM_A8:
				c = gh_disas_imm8(fp, b, e);
				if (c < 0) goto end;
				(void) fprintf(fp, ", a");
				break;
			case GH_VM_MOV_IMM_A16:
				c = gh_disas_imm16(fp, b, e);
				if (c < 0) goto end;
				(void) fprintf(fp, ", a");
				break;
			case GH_VM_MOV_IMM_A32:
				c = gh_disas_imm32(fp, b, e);
				if (c < 0) goto end;
				(void) fprintf(fp, ", a");
				break;
			case GH_VM_MOV_IMM_A64:
				c = gh_disas_imm64(fp, b, e);
				if (c < 0) goto end;
				(void) fprintf(fp, ", a");
				break;

			SINGLE_A(GH_VM_NEG_A8);
			SINGLE_A(GH_VM_BNEG_A8);

			case GH_VM_ADD_SP:
				c = gh_disas_offset(fp, b, e);
				if (c < 0) goto end;
				break;

			case GH_VM_JZ8:
			case GH_VM_JZ16:
			case GH_VM_JZ32:
			case GH_VM_JZ64:
			case GH_VM_JMP:
				c = gh_disas_addr(fp, b, e);
				if (c < 0) goto end;
				break;

			default:
				break;
		}
		fputc('\n', fp);
	}
end:
}

void gh_disas(FILE *fp, gh_bytecode *bc) {
	for (u64 i = 0; i < bc->fun.nfuns; i++)
		gh_disas_func(fp, bc, &bc->fun.funs[i]);
}
