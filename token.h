#ifndef _GALACH_TOKEN_H
#define _GALACH_TOKEN_H

#include <stdio.h>
#include "types.h"

typedef struct {
	enum gh_token_id {
		GH_TOK_KW_UNIT,
		GH_TOK_KW_I8,
		GH_TOK_KW_U8,
		GH_TOK_KW_I16,
		GH_TOK_KW_U16,
		GH_TOK_KW_I32,
		GH_TOK_KW_U32,
		GH_TOK_KW_I64,
		GH_TOK_KW_U64,
		GH_TOK_KW_F32,
		GH_TOK_KW_F64,

		GH_TOK_KW_VAR,
		GH_TOK_KW_FUN,

		GH_TOK_KW_WHILE,
		GH_TOK_KW_IF,
		GH_TOK_KW_THEN,
		GH_TOK_KW_ELSE,
		GH_TOK_KW_MATCH,
		GH_TOK_KW_RETURN,

		GH_TOK_KW_BEGIN,
		GH_TOK_KW_END,

		GH_TOK_KW_TRUE,
		GH_TOK_KW_FALSE,

		GH_TOK_IDENT,
		GH_TOK_LIT_INT,
		GH_TOK_LIT_FLOAT,
		GH_TOK_LIT_STRING,

		GH_TOK_RARROW,
		GH_TOK_COLON,

		GH_TOK_ASSIGN,
		GH_TOK_PLUS,
		GH_TOK_MINUS,
		GH_TOK_MULT,
		GH_TOK_DIV,
		GH_TOK_MODULO,
		GH_TOK_PLUS_ASSIGN,
		GH_TOK_MINUS_ASSIGN,
		GH_TOK_MULT_ASSIGN,
		GH_TOK_DIV_ASSIGN,
		GH_TOK_MODULO_ASSIGN,

		GH_TOK_EQ,
		GH_TOK_NEQ,
		GH_TOK_GT,
		GH_TOK_LT,
		GH_TOK_GEQ,
		GH_TOK_LEQ,

		GH_TOK_NEG,
		GH_TOK_AND,
		GH_TOK_OR,

		GH_TOK_BNEG,
		GH_TOK_BAND,
		GH_TOK_BOR,
		GH_TOK_BXOR,
		GH_TOK_LSHIFT,
		GH_TOK_RSHIFT,
		
		GH_TOK_LPAREN,
		GH_TOK_RPAREN,

		GH_TOK_COMMA,
		GH_TOK_EOF,
	} id;

	union {
		char *str;
		double flt;
		u64 i;
	} info;

	u64 lineno;
	u64 colno;
} gh_token;

gh_token *gh_token_init(char *c);
void gh_token_deinit(gh_token *tokens);

void gh_token_print(FILE *fp, gh_token *token);
void gh_token_debug(FILE *fp, gh_token *tokens);
void gh_token_debug_len(FILE *fp, gh_token *tokens, u64 ntokens);

#ifdef TEST
int gh_token_cmp(gh_token *one, gh_token *two);
#endif // TEST
#endif // _GALACH_TOKEN_H
