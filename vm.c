#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vm.h"
#include "log.h"

int gh_vm_init(gh_vm *vm, gh_bytecode *bytecode) {
	(void) vm; (void) bytecode;
	/*
	const u64 stack_bs = 2048;
	*vm = (gh_vm) {};
	vm->bc = bytecode;
	vm->stack.b = malloc(stack_bs);
	if (!vm->stack.b) {
		gh_log(GH_LOG_ERR, "malloc: %s", strerror(errno));
		return ;
	}*/
	return 0;
}

void gh_vm_run(gh_vm *vm) {
	(void) vm;
}

void gh_vm_debug(FILE *fp, gh_vm *vm) {
	(void) fp; (void) vm;
}

void gh_vm_deinit(gh_vm *vm) {
	(void) vm;
}
