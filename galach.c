/*
* Galach Language
*
* Designed and implemented by Zachary Gadad (2025)
*/

#ifdef TEST
#include "tests.h"
int main(int argc, char **argv) {
	(void) argc; (void) argv;
	gh_tests_run();
	return 0;
}
#else
#include "token.h"
#include "bytecode.h"
#include "vm.h"
#include "debug.h"

static void gh_parse_opt(char **arg) {
	(void) arg;
}

int main(int argc, char **argv) {
	gh_bytecode bytecode;
	gh_bytecode_init(&bytecode);

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-')
			gh_parse_opt(&argv[i]);
		else
			if (gh_bytecode_src(&bytecode, argv[i]) < 0)
				return -1;
	}
	gh_disas(stderr, &bytecode);
/*
	gh_vm vm;
	if (gh_vm_init(&vm, &bytecode) < 0)
		return -1;

	gh_vm_run(&vm);
	gh_vm_deinit(&vm);
*/
	gh_bytecode_deinit(&bytecode);
	return 0;
}

#endif // TEST
