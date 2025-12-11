/*
* Galach Language
*
* Designed and implemented by Zachary Gadad (2025)
*/

#include "token.h"
#include "bytecode.h"
#include "vm.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>

static void usage() {
	(void) fprintf(stderr,
		"The Galach programming language\n"
		"Specify source(s) and any options\n"
		"example: ./galach -d main.glc\n"
	);
	exit(EXIT_FAILURE);
}

static u8 opt_disas;
static void gh_parse_opt(char *arg) {
	switch (arg[1]) {
		case 'd': opt_disas = 1; break;
		default: usage();
	}
}

int main(int argc, char **argv) {
	gh_bytecode bytecode;
	gh_bytecode_init(&bytecode);

	int nsources = 0;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			gh_parse_opt(argv[i]);
		} else {
			nsources++;
			if (gh_bytecode_src(&bytecode, argv[i]) < 0)
				return -1;
		}
	}

	if (!nsources)
		usage();

	if (opt_disas)
		gh_disas(stderr, &bytecode);

	gh_vm vm;
	gh_vm_init(&vm, &bytecode);

	gh_vm_run(&vm);
	gh_vm_deinit(&vm);

	gh_bytecode_deinit(&bytecode);
	return 0;
}
