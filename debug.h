#ifndef _GALACH_DEBUG_H
#define _GALACH_DEBUG_H

#include "token.h"
#include "ast.h"
#include "bytecode.h"
#include "vm.h"

#include <stdio.h>

void gh_token_print(FILE *fp, gh_token *token);
void gh_token_debug(FILE *fp, token_v tokens);
void gh_ast_debug(gh_ast *root);
void gh_disas(FILE *fp, gh_bytecode *bc);

#endif // _GALACH_DEBUG_H
