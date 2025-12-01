#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "token.h"
#include "log.h"

static struct gh_keyword_map {
	enum gh_token_id id;
	const char *str;
} keyword_map[] = {
	{GH_TOK_KW_UNIT, "unit"},
	{GH_TOK_KW_I8, "i8"},
	{GH_TOK_KW_U8, "u8"},
	{GH_TOK_KW_I16, "i16"},
	{GH_TOK_KW_U16, "u16"},
	{GH_TOK_KW_I32, "i32"}, 
	{GH_TOK_KW_U32, "u32"},
	{GH_TOK_KW_I64, "i64"},
	{GH_TOK_KW_U64, "u64"},
	{GH_TOK_KW_F32, "f32"},
	{GH_TOK_KW_F64, "f64"},

	{GH_TOK_KW_VAR, "var"},
	{GH_TOK_KW_FUN, "fun"},

	{GH_TOK_KW_WHILE, "while"},
	{GH_TOK_KW_IF, "if"},
	{GH_TOK_KW_THEN, "then"},
	{GH_TOK_KW_ELSE, "else"},
	{GH_TOK_KW_MATCH, "match"},
	{GH_TOK_KW_RETURN, "return"},

	{GH_TOK_KW_BEGIN, "begin"},
	{GH_TOK_KW_END, "end"},

	{GH_TOK_KW_TRUE, "true"},
	{GH_TOK_KW_FALSE, "false"},
};
const u64 keyword_map_size = sizeof(keyword_map)/sizeof(struct gh_keyword_map);

static void gh_token_deinit_len(gh_token *tokens, u64 size);

static inline int gh_is_ws(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline int gh_is_digit(char c) {
	return (c >= '0' && c <= '9');
}

static inline int gh_is_alpha_start(char c) {
	return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int gh_is_alpha_cont(char c) {
	return gh_is_alpha_start(c) || gh_is_digit(c);
}

static int gh_parse_string(gh_token *token, char **c) {
	u64 size = 1;
	char *e = ++*c;
	while (*e >= ' ' && *e <= '~' && *e != '"') {
		if (*e == '\\') e++;
		e++, size++;
	}
	if (*e != '"') {
		gh_log(GH_LOG_ERR, "unterminated string literal");
		return -1;
	}

	token->info.str = malloc(size);
	if (!token->info.str) {
		gh_log(GH_LOG_ERR, "alloc string: %s", strerror(errno));
		return -1;
	}
	size = 0;
	while (*c < e) {
		if (**c == '\\') {
			switch (*++(*c)) {
				case 'a': token->info.str[size] = '\a'; break;
				case 'b': token->info.str[size] = '\b'; break;
				case 'n': token->info.str[size] = '\n'; break;
				case 't': token->info.str[size] = '\t'; break;
				case 'r': token->info.str[size] = '\r'; break;
				case '\\': token->info.str[size] = '\\'; break;
				default: {
					gh_log(GH_LOG_ERR, "invalid escape sequence: \\%c", **c);
					free(token->info.str);
					return -1;
				}
			}
		} else {
			token->info.str[size] = **c;
		}
		(*c)++, size++;
	}
	token->id = GH_TOK_LIT_STRING;
	return 0;
}

static void gh_parse_number(gh_token *token, char **c) {
	u64 x = 0;
	while (gh_is_digit(**c)) {
		x = x*10 + (**c - '0');
		(*c)++;
	}
	if (**c == '.') {
		(*c)++;
		double count = 10;
		double y = (double) x;
		while (gh_is_digit(**c)) {
			y += (**c - '0') / count;
			count *= 10, (*c)++;
		}
		token->id = GH_TOK_LIT_FLOAT;
		token->info.flt = y;
	} else {
		token->id = GH_TOK_LIT_INT;
		token->info.i = x;
	}
	(*c)--;
}

static void gh_parse_alpha_str(u64 *size, char **c) {
	*size = 0;
	while (gh_is_alpha_cont(**c))
		(*c)++, (*size)++;
	(*c)--;
}

static inline int gh_cmp_kw(const char *kw, char *start, u64 size) {
	u64 i;
	for (i = 0; i < size && kw[i] != 0; i++) {
		if (kw[i] != start[i]) return 0;
	}

	if (kw[i] != 0) return 0;
	return 1;
}

static int gh_parse_kw_or_ident(gh_token *token, char *start, u64 size) {
	for (u64 i = 0; i < keyword_map_size; i++) {
		if (gh_cmp_kw(keyword_map[i].str, start, size)) {
			token->id = keyword_map[i].id;
			return 0;
		}
	}

	token->id = GH_TOK_IDENT;
	token->info.str = strndup(start, size);
	if (!token->info.str) {
		gh_log(GH_LOG_ERR, "alloc identifier: %s", strerror(errno));
		return -1;
	}
	return 0;
}

gh_token *gh_token_init(char *c) {
	u64 lineno = 1;
	char *linestart = c;

	const u64 bs = 32;
	u64 used = 0;
	u64 cap  = bs;
	gh_token *tokens = malloc(cap * sizeof(gh_token));
	if (!tokens) {
		gh_log(GH_LOG_ERR, "alloc tokens: %s", strerror(errno));
		return NULL;
	}

	for (;;) {
		if (gh_is_ws(*c)) {
			if (*c == '\n') {
				lineno++;
				linestart = c+1;
			}
			c++;
			continue;
		}

		if (used == cap) {
			cap += bs;
			gh_token *nt = realloc(tokens, cap * sizeof(gh_token));
			if (!nt) {
				gh_log(GH_LOG_ERR, "realloc tokens: %s", strerror(errno));
				goto e0;
			}
			tokens = nt;
		}

		#define CASE(x) case x: switch(*(c+1)) {
		#define ALT(x, t) case x: token.id = t; c++; break;
		#define DFLT(t) default: token.id = t; break; } break;
		gh_token token = (gh_token) {
			.lineno = lineno,
			.colno = (u64) (c - linestart) + 1,
		};
		switch (*c) {
			CASE('=') ALT('=', GH_TOK_EQ)            DFLT(GH_TOK_ASSIGN);
			CASE('+') ALT('=', GH_TOK_PLUS_ASSIGN)   DFLT(GH_TOK_PLUS);
			CASE('*') ALT('=', GH_TOK_MULT_ASSIGN)   DFLT(GH_TOK_MULT);
			CASE('/') ALT('=', GH_TOK_DIV_ASSIGN)    DFLT(GH_TOK_DIV);
			CASE('%') ALT('=', GH_TOK_MODULO_ASSIGN) DFLT(GH_TOK_MODULO);
			CASE('!') ALT('=', GH_TOK_NEQ)           DFLT(GH_TOK_NEG);
			CASE('-')
				ALT('>', GH_TOK_RARROW)
				ALT('=', GH_TOK_MINUS_ASSIGN)
			DFLT(GH_TOK_MINUS);
			CASE('>')
				ALT('=', GH_TOK_GEQ)
				ALT('>', GH_TOK_RSHIFT)
			DFLT(GH_TOK_GT);
			CASE('<')
				ALT('=', GH_TOK_LEQ)
				ALT('<', GH_TOK_LSHIFT)
			DFLT(GH_TOK_LT);
			CASE('&') ALT('&', GH_TOK_AND) DFLT(GH_TOK_BAND);
			CASE('|') ALT('|', GH_TOK_OR)  DFLT(GH_TOK_BOR);
			CASE('^') DFLT(GH_TOK_BXOR);
			CASE('~') DFLT(GH_TOK_BNEG);
			CASE('(') DFLT(GH_TOK_LPAREN);
			CASE(')') DFLT(GH_TOK_RPAREN);
			CASE(',') DFLT(GH_TOK_COMMA);
			CASE(':') DFLT(GH_TOK_COLON);
			case '\"': {
				if (gh_parse_string(&token, &c) < 0) goto e0;
				break;
			}
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9': case '.': {
				gh_parse_number(&token, &c);
				break;
			}
			case '\0': {
				token.id = GH_TOK_EOF;
				break;
			}
			default: {
				if (gh_is_alpha_start(*c)) {
					u64 size;
					char *start = c;
					gh_parse_alpha_str(&size, &c);
					if (gh_parse_kw_or_ident(&token, start, size) < 0) goto e0;
				} else {
					gh_log(GH_LOG_ERR, "invalid char: %c", *c);
					goto e0;
				}
			}
		}

		tokens[used++] = token;
		if (token.id == GH_TOK_EOF)
			break;
		c++;
	}

	return tokens;

e0:
	gh_token_deinit_len(tokens, used);
	return NULL;
}

static void gh_token_free(gh_token *token) {
	if (token->id == GH_TOK_IDENT || token->id == GH_TOK_LIT_STRING)
		free(token->info.str);
}

static void gh_token_deinit_len(gh_token *tokens, u64 size) {
	if (!tokens) return ;
	for (u64 i = 0; i < size; i++)
		gh_token_free(&tokens[i]);
	free(tokens);
}

void gh_token_deinit(gh_token *tokens) {
	if (!tokens) return ;
	gh_token *start = tokens;
	while (tokens->id != GH_TOK_EOF)
		gh_token_free(tokens++);
	free(start);
}

#ifdef TEST
int gh_token_cmp(gh_token *one, gh_token *two) {
	while (one->id != GH_TOK_EOF) {
		if (one->id != two->id) return 0;
		switch (one->id) {
			case GH_TOK_IDENT:
			case GH_TOK_LIT_STRING:
				if (strcmp(one->info.str, two->info.str)) return 0;
				break;
			case GH_TOK_LIT_INT:
				if (one->info.i != two->info.i) return 0;
				break;
			case GH_TOK_LIT_FLOAT:
				if (one->info.flt != two->info.flt) return 0;
				break;
			default: break;
		}
		one++, two++;
	}
	return 1;
}
#endif // TEST
