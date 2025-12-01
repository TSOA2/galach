#ifdef TEST
#include <stdio.h>
#include "tests.h"
#include "debug.h"
#include "types.h"
#include "log.h"

#define FAIL_TEST(name) { \
	gh_log(GH_LOG_ERR, "FAILED test \"" #name "\""); \
	failed++; \
}

#define RUN_TOK_TEST(in, ...) { \
	completed++; \
	gh_log(GH_LOG_INFO, "running token test..."); \
	gh_token *toks = gh_token_init(in); \
	if (!toks) { FAIL_TEST(token); } \
	else { \
		gh_token exp[] = {__VA_ARGS__}; \
		if (!gh_token_cmp(toks, exp)) { \
			gh_log(GH_LOG_ERR, "\n=== expected: "); \
			gh_token_debug_len(stderr, exp, sizeof(exp)/sizeof(gh_token)); \
			(void) fprintf(stderr, "=== got: \n"); \
			gh_token_debug(stderr, toks); \
			FAIL_TEST(token); \
		} \
		gh_token_deinit(toks); \
	} \
}

static u64 completed = 0;
static u64 failed = 0;
void gh_tests_run(void) {
	RUN_TOK_TEST("123", ((gh_token){.id=GH_TOK_LIT_INT, .info={.i=123}}));
	RUN_TOK_TEST("123.5", ((gh_token){.id=GH_TOK_LIT_FLOAT, .info={.flt=123.5}}));
	RUN_TOK_TEST(".5", ((gh_token){.id=GH_TOK_LIT_FLOAT, .info={.flt=.5}}));
	RUN_TOK_TEST("\"test\"", ((gh_token){.id=GH_TOK_LIT_STRING, .info={.str="test"}}));
	RUN_TOK_TEST("test", ((gh_token){.id=GH_TOK_IDENT, .info={.str="test"}}));
	RUN_TOK_TEST("(", ((gh_token){.id=GH_TOK_LPAREN}));
	RUN_TOK_TEST(")", ((gh_token){.id=GH_TOK_RPAREN}));
	RUN_TOK_TEST("-=", ((gh_token){.id=GH_TOK_MINUS_ASSIGN}));
	RUN_TOK_TEST("+=", ((gh_token){.id=GH_TOK_PLUS_ASSIGN}));
	RUN_TOK_TEST("->", ((gh_token){.id=GH_TOK_RARROW}));
	RUN_TOK_TEST("-", ((gh_token){.id=GH_TOK_MINUS}));
	RUN_TOK_TEST("123hi", 
			  ((gh_token){.id=GH_TOK_LIT_INT, .info={.i=123}}),
			  ((gh_token){.id=GH_TOK_IDENT,   .info={.str="hi"}}));
	RUN_TOK_TEST("\t  ~THING\n",
			  ((gh_token){.id=GH_TOK_BNEG}),
			  ((gh_token){.id=GH_TOK_IDENT, .info={.str="THING"}}));
	RUN_TOK_TEST("fun fizzbuzz() -> unit",
			  ((gh_token){.id=GH_TOK_KW_FUN}),
			  ((gh_token){.id=GH_TOK_IDENT, .info={.str="fizzbuzz"}}),
			  ((gh_token){.id=GH_TOK_LPAREN}),
			  ((gh_token){.id=GH_TOK_RPAREN}),
			  ((gh_token){.id=GH_TOK_RARROW}),
			  ((gh_token){.id=GH_TOK_KW_UNIT}));
	RUN_TOK_TEST("_ident123", ((gh_token){.id=GH_TOK_IDENT, .info={.str="_ident123"}}));
	RUN_TOK_TEST("|", ((gh_token){.id=GH_TOK_BOR}));
	RUN_TOK_TEST("||", ((gh_token){.id=GH_TOK_OR}));
	RUN_TOK_TEST("&", ((gh_token){.id=GH_TOK_BAND}));
	RUN_TOK_TEST("&&", ((gh_token){.id=GH_TOK_AND}));
	RUN_TOK_TEST("<<", ((gh_token){.id=GH_TOK_LSHIFT}));
	RUN_TOK_TEST(">>", ((gh_token){.id=GH_TOK_RSHIFT}));
	RUN_TOK_TEST("\"\\a \\n\"", ((gh_token){.id=GH_TOK_LIT_STRING, .info={.str="\a \n"}}));

	gh_log(GH_LOG_INFO, "%llu/%llu tests failed", failed, completed);

	gh_token *toks = gh_token_init(
	"fun thing() -> unit begin\n"
		"\tvar count : i32 = 1\n"
	"end\n");
	if (toks) {
		gh_ast *ast = gh_ast_init(toks);
		if (ast) {
			gh_bytecode bc;
			gh_bytecode_init(&bc);
			gh_bytecode_mem(&bc, ast);
			gh_disas(stderr, &bc);
			gh_bytecode_deinit(&bc);
			gh_ast_deinit(ast);
		} else {
			gh_log(GH_LOG_ERR, "failed to get ast");
		}
		gh_token_deinit(toks);
	} else {
		gh_log(GH_LOG_ERR, "failed to get tokens");
	}
}

#endif // TEST
