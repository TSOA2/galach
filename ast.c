#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "log.h"

#define TRY(p, x, e) do { \
	void *_p = (void *) (p = x); \
	if (!_p) goto e; \
} while (0)

#define D() do { \
	(void)fprintf(stderr, "debug(%s): ", __FUNCTION__); \
	gh_token_print(stderr, *tidx); \
	(void) fputc('\n', stderr);\
} while (0)

static inline gh_ast *ALLOC_NODE(enum gh_ast_type type) {
	gh_ast *ptr = calloc(1, sizeof(gh_ast));
	if (UNLIKELY(!ptr)) return NULL;
	ptr->type = type;
	return ptr;
}

static inline void DEALLOC_NODE(gh_ast *ast) {
	if (LIKELY(ast))
		free(ast);
}

#define EXPECT(token, exp, label) do { \
	gh_token *_token = (token); \
	enum gh_token_id _exp = (exp); \
	if (_token->id != _exp) { \
		if (!is_optional) { \
			(void) fprintf(stderr, "function %s: ", __FUNCTION__);\
			gh_ast_errtokens(_token, &(gh_token){.id=_exp}); \
		} \
		goto label; \
	} \
} while (0)

#define EXPECT_TYPE(token, label) do { \
	gh_token *_token = (token); \
	switch (_token->id) { \
		case GH_TOK_KW_UNIT: \
		case GH_TOK_KW_I8:  \
		case GH_TOK_KW_U8:  \
		case GH_TOK_KW_I16: \
		case GH_TOK_KW_U16: \
		case GH_TOK_KW_I32: \
		case GH_TOK_KW_U32: \
		case GH_TOK_KW_I64: \
		case GH_TOK_KW_U64: \
		case GH_TOK_KW_F32: \
		case GH_TOK_KW_F64: break; \
		default: \
			if (!is_optional) \
				gh_ast_errtype(_token); \
			goto label; \
	} \
} while (0)

#define GH_AST_ERR_FP (stderr)

static int is_optional;
static void gh_ast_errtoken(gh_token *got) {
	(void) fprintf(stderr, "line: %llu, col: %llu: ",
				got->lineno, got->colno);
}

static void gh_ast_erreof(gh_token *got) {
	gh_ast_errtoken(got);
	(void) fprintf(stderr, "expected EOF, got '");
	gh_token_print(stderr, got);
	(void) fprintf(stderr, "'\n");
}

static void gh_ast_errtokens(gh_token *got, gh_token *expected) {
	gh_ast_errtoken(got);
	(void) fprintf(stderr, "expected token '");
	gh_token_print(stderr, expected);
	(void) fprintf(stderr, "', got token '");
	gh_token_print(stderr, got);
	(void) fprintf(stderr, "'\n");
}

static void gh_ast_errtype(gh_token *token) {
	gh_ast_errtoken(token);
	(void) fprintf(stderr, "expected type\n");
}

static void gh_ast_errexpr(gh_token *token) {
	gh_ast_errtoken(token);
	(void) fprintf(stderr, "expected expression\n");
}

static gh_ast *gh_ast_parse_block(gh_token **tidx);
static gh_ast *gh_ast_parse_statement(gh_token **tidx);
static gh_ast *gh_ast_parse_if(gh_token **tidx);
static gh_ast *gh_ast_parse_expr(gh_token **tidx);

#define PARSE_BRANCH(name, token, type, succ) \
static gh_ast *gh_ast_parse_ ## name (gh_token **tidx) { \
	gh_ast *child = NULL; \
	TRY(child, gh_ast_parse_ ## succ (tidx), e0); \
	if ((*tidx)->id == token) { \
		gh_ast *tmp = NULL; \
		TRY(tmp, ALLOC_NODE(type), e0); \
		tmp->branch.first = child; \
		child = tmp; \
		(*tidx)++; \
		TRY(child->branch.second, gh_ast_parse_ ## name (tidx), e0); \
	} \
	return child; \
e0: \
	gh_ast_deinit(child); \
	return NULL; \
}

#define PARSE_BRANCH_OP(name, tokens, type, succ) \
static gh_ast *gh_ast_parse_ ## name (gh_token **tidx) { \
	gh_ast *child = NULL; \
	TRY(child, gh_ast_parse_ ## succ (tidx), e0); \
	switch ((*tidx)->id) { \
		tokens { \
			gh_ast *tmp = NULL; \
			TRY(tmp, ALLOC_NODE(type), e0); \
			tmp->branch_op.op = (*tidx)++; \
			tmp->branch_op.first = child; \
			child = tmp; \
			TRY(child->branch_op.second, gh_ast_parse_ ## name (tidx), e0); \
			break; \
		} \
		default: break; \
	} \
	return child; \
e0: \
	gh_ast_deinit(child); \
	return NULL; \
}

static gh_ast *gh_ast_parse_clist(gh_token **tidx) {
	gh_ast *root = NULL;
	gh_ast **idx = &root;
	for (;; (*tidx)++) {
		TRY(*idx, ALLOC_NODE(GH_AST_CLIST), e0);
		if ((*tidx)->id == GH_TOK_RPAREN)
			break;
		TRY((*idx)->clist.expr, gh_ast_parse_expr(tidx), e0);
		if ((*tidx)->id == GH_TOK_COMMA)
			idx = &(*idx)->clist.clist;
		else
			break;
	}
	EXPECT((*tidx)++, GH_TOK_RPAREN, e0);
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_primary(gh_token **tidx) {
	gh_ast *root = NULL;
	switch ((*tidx)->id) {
		case GH_TOK_IDENT:
		case GH_TOK_LIT_INT:
		case GH_TOK_LIT_FLOAT: case GH_TOK_LIT_STRING: {
			gh_token *literal = (*tidx)++;
			TRY(root, ALLOC_NODE(GH_AST_PRIMARY), e0);
			root->primary.literal = literal;
			if (literal->id == GH_TOK_IDENT && (*tidx)->id == GH_TOK_LPAREN) {
				(*tidx)++;
				root->primary.clist = gh_ast_parse_clist(tidx);
			}
			break;
		}
		default: {
			EXPECT((*tidx)++, GH_TOK_LPAREN, e0);
			TRY(root, gh_ast_parse_expr(tidx), e0);
			EXPECT((*tidx)++, GH_TOK_RPAREN, e0);
			break;
		}
	}

	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_unary(gh_token **tidx) {
	gh_ast *child = NULL;
	switch ((*tidx)->id) {
		case GH_TOK_NEG: case GH_TOK_BNEG:
		case GH_TOK_MINUS: {
			TRY(child, ALLOC_NODE(GH_AST_UNARY), e0);
			child->unary.op = (*tidx)++;
			child->unary.child = gh_ast_parse_unary(tidx);
			break;
		}
		default: child = gh_ast_parse_primary(tidx); break;
	}
	return child;
e0:
	return NULL;
}

PARSE_BRANCH_OP(factor,
				case GH_TOK_MULT: case GH_TOK_DIV: case GH_TOK_MODULO:,
				GH_AST_FACTOR, unary)
PARSE_BRANCH_OP(adder, case GH_TOK_PLUS: case GH_TOK_MINUS:, GH_AST_ADDER, factor)
PARSE_BRANCH_OP(shifter, case GH_TOK_LSHIFT: case GH_TOK_RSHIFT:, GH_AST_SHIFTER, adder)
PARSE_BRANCH_OP(relation,
				case GH_TOK_GT: case GH_TOK_LT:
				case GH_TOK_GEQ: case GH_TOK_LEQ:,
				GH_AST_RELATION, shifter)
PARSE_BRANCH_OP(compare, case GH_TOK_EQ: case GH_TOK_NEQ:, GH_AST_COMPARE, relation)
PARSE_BRANCH(band, GH_TOK_BAND, GH_AST_BAND, compare)
PARSE_BRANCH(bxor, GH_TOK_BXOR, GH_AST_BXOR, band)
PARSE_BRANCH(bor,  GH_TOK_BOR,  GH_AST_BOR,  bxor)
PARSE_BRANCH(and,  GH_TOK_AND,  GH_AST_AND,  bor)
PARSE_BRANCH(or,   GH_TOK_OR,   GH_AST_OR,   and)

static gh_ast *gh_ast_parse_assgn(gh_token **tidx) {
	gh_token *start = *tidx;
	gh_ast *root = NULL;
	gh_token *ident = NULL;
	EXPECT(*tidx, GH_TOK_IDENT, e0);
	ident = *tidx;
	switch ((*tidx + 1)->id) {
		case GH_TOK_ASSIGN: case GH_TOK_PLUS_ASSIGN:
		case GH_TOK_MINUS_ASSIGN: case GH_TOK_MULT_ASSIGN:
		case GH_TOK_DIV_ASSIGN: case GH_TOK_MODULO_ASSIGN: {
			(*tidx)++;
			TRY(root, ALLOC_NODE(GH_AST_ASSGN), e0);
			root->assgn.ident = ident;
			root->assgn.op    = (*tidx)++;
			TRY(root->assgn.expr, gh_ast_parse_expr(tidx), e0);
			break;
		}
		default: goto e0;
	}
	return root;
e0:
	if (is_optional)
		*tidx = start;
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_expr(gh_token **tidx) {
	gh_ast *child;
	is_optional = 1;
	if ((child = gh_ast_parse_assgn(tidx)))
		return child;

	is_optional = 0;
	if ((child = gh_ast_parse_or(tidx)))
		return child;

	gh_ast_errexpr(*tidx);
	return NULL;
}

static gh_ast *gh_ast_parse_var(gh_token **tidx) {
	gh_ast *root = NULL;
	TRY(root, ALLOC_NODE(GH_AST_VAR), e0);
	EXPECT((*tidx)++, GH_TOK_KW_VAR, e0);
	EXPECT(*tidx, GH_TOK_IDENT, e0);
	root->var.ident = (*tidx)++;
	EXPECT((*tidx)++, GH_TOK_COLON, e0);
	EXPECT_TYPE(*tidx, e0);
	root->var.type = (*tidx)++;
	if ((*tidx)->id == GH_TOK_ASSIGN) {
		(*tidx)++;
		TRY(root->var.expr, gh_ast_parse_expr(tidx), e0);
	}
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_if(gh_token **tidx) {
	gh_ast *root = NULL;
	EXPECT((*tidx)++, GH_TOK_KW_IF, e0);
	TRY(root, ALLOC_NODE(GH_AST_IF), e0);
	TRY(root->ifexpr.expr, gh_ast_parse_expr(tidx), e0);
	EXPECT((*tidx)++, GH_TOK_KW_THEN, e0);
	TRY(root->ifexpr.statement, gh_ast_parse_statement(tidx), e0);
	if ((*tidx)->id == GH_TOK_KW_ELSE) {
		(*tidx)++;
		int is_elseif = (*tidx)->id == GH_TOK_KW_IF;
		TRY(root->ifexpr.endif, gh_ast_parse_statement(tidx), e0);
		if (!is_elseif)
			EXPECT((*tidx)++, GH_TOK_KW_END, e0);
	} else {
		EXPECT((*tidx)++, GH_TOK_KW_END, e0);
	}
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_mselect(gh_token **tidx) {
	gh_ast *root = NULL;
	gh_ast **idx = &root;

	for (;;) {
		TRY(*idx, ALLOC_NODE(GH_AST_MSELECT), e0);
		TRY((*idx)->mselect.expr, gh_ast_parse_expr(tidx), e0);
		EXPECT((*tidx)++, GH_TOK_KW_THEN, e0);
		TRY((*idx)->mselect.statement, gh_ast_parse_statement(tidx), e0);
		EXPECT((*tidx)++, GH_TOK_KW_END, e0);
		if ((*tidx)->id == GH_TOK_KW_END)
			break;
		idx = &(*idx)->mselect.mselect;
	}
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_match(gh_token **tidx) {
	gh_ast *root = NULL;
	EXPECT((*tidx)++, GH_TOK_KW_MATCH, e0);
	TRY(root, ALLOC_NODE(GH_AST_MATCH), e0);
	if ((*tidx)->id != GH_TOK_KW_BEGIN)
		root->match.expr = gh_ast_parse_expr(tidx);
	EXPECT((*tidx)++, GH_TOK_KW_BEGIN, e0);
	TRY(root->match.mselect, gh_ast_parse_mselect(tidx), e0);
	EXPECT((*tidx)++, GH_TOK_KW_END, e0);
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_while(gh_token **tidx) {
	gh_ast *root = NULL;
	EXPECT((*tidx)++, GH_TOK_KW_WHILE, e0);
	TRY(root, ALLOC_NODE(GH_AST_WHILE), e0);
	TRY(root->whileexpr.expr, gh_ast_parse_expr(tidx), e0);
	TRY(root->whileexpr.block, gh_ast_parse_block(tidx), e0);
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_return(gh_token **tidx) {
	gh_ast *root = NULL;
	EXPECT((*tidx)++, GH_TOK_KW_RETURN, e0);
	TRY(root, ALLOC_NODE(GH_AST_RETURN), e0);
	TRY(root->returnexpr.expr, gh_ast_parse_expr(tidx), e0);
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static int gh_ast_is_end(gh_token *tidx) {
	return tidx->id == GH_TOK_KW_END || tidx->id == GH_TOK_KW_ELSE;
}

static gh_ast *gh_ast_parse_statement(gh_token **tidx) {
	gh_ast *root = NULL;
	gh_ast **idx = &root;
	for (;;) {
		if (gh_ast_is_end(*tidx))
			break;

		TRY(*idx, ALLOC_NODE(GH_AST_STATEMENT), e0);
		gh_ast **child = &(*idx)->statement.child;
		switch ((*tidx)->id) {
			case GH_TOK_KW_VAR: TRY(*child, gh_ast_parse_var(tidx), e0); break;
			case GH_TOK_KW_IF: TRY(*child, gh_ast_parse_if(tidx), e0); break;
			case GH_TOK_KW_MATCH: TRY(*child, gh_ast_parse_match(tidx), e0); break;
			case GH_TOK_KW_WHILE: TRY(*child, gh_ast_parse_while(tidx), e0); break;
			case GH_TOK_KW_RETURN: TRY(*child, gh_ast_parse_return(tidx), e0); break;
			case GH_TOK_KW_BEGIN: TRY(*child, gh_ast_parse_block(tidx), e0); break;
			default:
				TRY(*child, gh_ast_parse_expr(tidx), e0);
		}
		idx = &(*idx)->statement.statement;
	}
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_block(gh_token **tidx) {
	gh_ast *root = NULL;
	EXPECT((*tidx)++, GH_TOK_KW_BEGIN, e0);
	root = gh_ast_parse_statement(tidx);
	EXPECT((*tidx)++, GH_TOK_KW_END, e0);
	return root;
e0:
	return NULL;
}

static gh_ast *gh_ast_parse_flist(gh_token **tidx) {
	gh_ast *root = NULL;
	gh_ast **idx = &root;
	if ((*tidx)->id == GH_TOK_RPAREN) return NULL;
	for (;; (*tidx)++) {
		TRY(*idx, ALLOC_NODE(GH_AST_FLIST), e0);
		EXPECT_TYPE(*tidx, e0);
		(*idx)->flist.type = (*tidx)++;
		EXPECT(*tidx, GH_TOK_IDENT, e0);
		(*idx)->flist.ident = (*tidx)++;
		if ((*tidx)->id == GH_TOK_COMMA)
			idx = &(*idx)->flist.flist;
		else
			break;
	}
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_fun(gh_token **tidx) {
	gh_ast *root = NULL;
	EXPECT((*tidx)++, GH_TOK_KW_FUN, e0);
	EXPECT(*tidx, GH_TOK_IDENT, e0);
	TRY(root, ALLOC_NODE(GH_AST_FUN), e0);
	root->fun.ident = (*tidx)++;
	EXPECT((*tidx)++, GH_TOK_LPAREN, e0);
	root->fun.flist = gh_ast_parse_flist(tidx);
	EXPECT((*tidx)++, GH_TOK_RPAREN, e0);
	EXPECT((*tidx)++, GH_TOK_RARROW, e0);
	EXPECT_TYPE(*tidx, e0);
	root->fun.type = (*tidx)++;
	root->fun.block = gh_ast_parse_block(tidx);
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

static gh_ast *gh_ast_parse_decl(gh_token **tidx) {
	gh_ast *root = NULL;
	gh_ast **idx = &root;
	for (;;) {
		if ((*tidx)->id == GH_TOK_EOF)
			goto end;

		TRY(*idx, ALLOC_NODE(GH_AST_DECL), e0);
		switch ((*tidx)->id) {
			case GH_TOK_KW_FUN: TRY((*idx)->decl.child, gh_ast_parse_fun(tidx), e0); break;
			case GH_TOK_KW_VAR: TRY((*idx)->decl.child, gh_ast_parse_var(tidx), e0); break;
			default: gh_ast_erreof(*tidx); goto e0;
		}
		idx = &(*idx)->decl.decl;
	}
end:
	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

gh_ast *gh_ast_init(gh_token *tokens) {
	gh_token **tidx = &tokens;
	gh_ast *root = NULL;

	TRY(root, ALLOC_NODE(GH_AST_PRGM), e0);
	TRY(root->prgm.decl, gh_ast_parse_decl(tidx), e0);

	return root;
e0:
	gh_ast_deinit(root);
	return NULL;
}

void gh_ast_deinit(gh_ast *root) {
	if (!root) return ;
	switch (root->type) {
		case GH_AST_PRGM: gh_ast_deinit(root->prgm.decl); break;
		case GH_AST_DECL:
			gh_ast_deinit(root->decl.child);
			gh_ast_deinit(root->decl.decl);
			break;
		case GH_AST_FUN:
			gh_ast_deinit(root->fun.flist);
			gh_ast_deinit(root->fun.block);
			break;
		case GH_AST_FLIST: gh_ast_deinit(root->flist.flist); break;
		case GH_AST_VAR: gh_ast_deinit(root->var.expr); break;
		case GH_AST_STATEMENT:
			gh_ast_deinit(root->statement.child);
			gh_ast_deinit(root->statement.statement);
			break;
		case GH_AST_IF:
			gh_ast_deinit(root->ifexpr.expr);
			gh_ast_deinit(root->ifexpr.statement);
			gh_ast_deinit(root->ifexpr.endif);
			break;
		case GH_AST_MATCH:
			gh_ast_deinit(root->match.expr);
			gh_ast_deinit(root->match.mselect); break;
		case GH_AST_MSELECT:
			gh_ast_deinit(root->mselect.expr);
			gh_ast_deinit(root->mselect.statement);
			gh_ast_deinit(root->mselect.mselect);
			break;
		case GH_AST_WHILE:
			gh_ast_deinit(root->whileexpr.expr);
			gh_ast_deinit(root->whileexpr.block);
			break;
		case GH_AST_RETURN:
			gh_ast_deinit(root->returnexpr.expr);
			break;
		case GH_AST_OR: case GH_AST_AND:
		case GH_AST_BOR: case GH_AST_BXOR:
		case GH_AST_BAND:
			gh_ast_deinit(root->branch.first);
			gh_ast_deinit(root->branch.second);
			break;
		case GH_AST_COMPARE: case GH_AST_RELATION:
		case GH_AST_SHIFTER: case GH_AST_ADDER:
		case GH_AST_FACTOR:
			gh_ast_deinit(root->branch_op.first);
			gh_ast_deinit(root->branch_op.second);
			break;
		case GH_AST_ASSGN:
			gh_ast_deinit(root->assgn.expr);
			break;
		case GH_AST_UNARY: gh_ast_deinit(root->unary.child); break;
		case GH_AST_PRIMARY: gh_ast_deinit(root->primary.clist); break;
		case GH_AST_CLIST:
			gh_ast_deinit(root->clist.expr);
			gh_ast_deinit(root->clist.clist);
			break;
		default: break;
	}
	DEALLOC_NODE(root);
}
