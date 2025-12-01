#ifndef _GALACH_AST_H
#define _GALACH_AST_H

#include "token.h"
#include "types.h"

/*
 * Things to do in order of priority:
 * - Remove unnecessary layers of indirection
 *   - These are AST nodes that point to only one other node and can be inferred.
 * - Flatten the AST (profile first)
 *   - This is doing a crazy amount of allocations
 *   - Maybe cache friendly? for our purposes that's not really important yet tho
 */

/*
 * program    -> decl EOF
 * decl       -> (function | var) decl
 * function   -> 'fun' IDENT '(' flist ')' '->' type block
 * flist      -> type IDENT ?(',' flist)
 * var        -> 'var' IDENT ':' type ?( '=' expression)
 *
 * statement  -> (var | if | match | while | block | expression) ?(statement)
 * block      -> 'begin' ?(statement) 'end'
 *
 * if         -> 'if' expression ifblock
 * ifblock    -> 'then' ?(statement) endif
 * endif      -> elseif | else | 'end'
 * elseif     -> 'else' if
 * else       -> 'else' ?(statement) 'end'
 *
 * match      -> 'match' ?(IDENT) 'begin' ?(mselect) 'end'
 * mselect    -> expression 'then' ?(statement) 'end' ?(mselect)
 *
 * while      -> 'while' expression block
 *
 * expression -> assignment | or
 * assignment -> IDENT ('=' | '+=' | '-=' | '*=' | '/=' | '%=') expression
 * or         -> and ?('||' or)
 * and        -> bor ?('&&' and)
 * bor        -> bxor ?('|' bor)
 * bxor       -> band ?('^' bxor)
 * band       -> compare ?('&' band)
 * compare    -> relational ?(('==' | '!=') compare)
 * relational -> shifter ?(('<' | '<=' | '>' | '>=') relational)
 * shifter    -> adder ?(('<<' | '>>') shifter)
 * adder      -> factor ?(('+' | '-') adder)
 * factor     -> unary ?(('*' | '/' | '%') factor)
 * unary      -> (('!' | | '~' | '-') unary) | primary
 * primary    -> IDENT | INT | FLOAT | STRING | '(' expression ')'
 */

typedef struct gh_ast {
	enum gh_ast_type {
		GH_AST_PRGM,      GH_AST_DECL,
		GH_AST_FUN,       GH_AST_FLIST,
		GH_AST_VAR,
		GH_AST_STATEMENT, GH_AST_CLIST,
		GH_AST_IF,        GH_AST_MATCH,
		GH_AST_MSELECT,   GH_AST_WHILE,
		GH_AST_ASSGN,     GH_AST_RETURN,
		GH_AST_OR,        GH_AST_AND,
		GH_AST_BOR,       GH_AST_BXOR,
		GH_AST_BAND,      GH_AST_COMPARE,
		GH_AST_RELATION,  GH_AST_SHIFTER,
		GH_AST_ADDER,     GH_AST_FACTOR,
		GH_AST_UNARY,     GH_AST_PRIMARY,
	} type;

	union {
		struct { struct gh_ast *decl;  } prgm;
		struct {
			struct gh_ast *child;
			struct gh_ast *decl; // optional
		} decl;
		struct {
			gh_token *ident;
			struct gh_ast *flist;
			gh_token *type;
			struct gh_ast *block;
		} fun;
		struct {
			gh_token *type;
			gh_token *ident;
			struct gh_ast *flist; // optional
		} flist;
		struct {
			gh_token *ident;
			gh_token *type;
			struct gh_ast *expr; // optional
		} var;
		struct {
			struct gh_ast *child;
			struct gh_ast *statement;
		} statement;
		struct {
			struct gh_ast *expr;
			struct gh_ast *statement;
			struct gh_ast *endif;
		} ifexpr;
		struct {
			struct gh_ast *expr;
			struct gh_ast *mselect; // optional
		} match;
		struct {
			struct gh_ast *expr;
			struct gh_ast *statement;
			struct gh_ast *mselect; // optional
		} mselect;
		struct {
			struct gh_ast *expr;
			struct gh_ast *block;
		} whileexpr;
		struct {
			struct gh_ast *expr;
		} returnexpr;

		struct {
			struct gh_ast *first;
			struct gh_ast *second;
		} branch;
		struct {
			gh_token *op;
			struct gh_ast *first;
			struct gh_ast *second;
		} branch_op;
		struct {
			gh_token *ident;
			gh_token *op;
			struct gh_ast *expr;
		} assgn;
		struct {
			gh_token *op;
			struct gh_ast *child;
		} unary;
		struct {
			gh_token *literal;
			struct gh_ast *clist; // for function calls
		} primary;
		struct {
			struct gh_ast *expr;
			struct gh_ast *clist;
		} clist;
	};
} gh_ast;

gh_ast *gh_ast_init(gh_token *tokens);
void gh_ast_debug(gh_ast *root);
void gh_ast_deinit(gh_ast *root);

#endif // _GALACH_AST_H
