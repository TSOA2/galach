#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include "vm.h"
#include "log.h"

#define fail() do { \
	gh_log(GH_LOG_ERR, \
		"vm crashed from %s\n" \
		"line: %d\n", __func__, __LINE__ \
	); \
	longjmp(fail_buf, 1); \
} while (0)

static jmp_buf fail_buf;

// The stack is backwards due to poor design decisions
// when making the bytecode...
//
// I didn't consider that I wanted a growable stack.
// One upside is that it might be easier to port to x86 at some point
//
// This code is very messy but I don't have enough time to make it any
// better, some macros should be removed or added

#define SP vm->stack.used
#define A8 ((u8)(vm->a&0xff))
#define A16 ((u16)(vm->a&0xffff))
#define A32 ((u32)(vm->a&0xffffffff))
#define A64 ((u64)(vm->a&0xffffffffffffffff))

void gh_vm_init(gh_vm *vm, gh_bytecode *bytecode) {
	*vm = (gh_vm) {};
	vm->bc = bytecode;
	vm->stack = INIT_VEC(u8);
}

static u8 get8(gh_vm *vm) {
	return vm->bc->bytes.data[vm->ip++];
}

static u16 get16(gh_vm *vm) {
	u16 res = ((u16)vm->bc->bytes.data[vm->ip] << 8)
	| ((u16)vm->bc->bytes.data[vm->ip+1]);
	vm->ip += 2;
	return res;
}

static u32 get32(gh_vm *vm) {
	u32 res = ((u32)vm->bc->bytes.data[vm->ip] << 24)
	| ((u32)vm->bc->bytes.data[vm->ip+1] << 16)
	| ((u32)vm->bc->bytes.data[vm->ip+2] << 8)
	| ((u32)vm->bc->bytes.data[vm->ip+3] << 0);
	vm->ip += 4;
	return res;
}

static u64 get64(gh_vm *vm) {
	u64 res = ((u64)vm->bc->bytes.data[vm->ip] << 56)
	| ((u64)vm->bc->bytes.data[vm->ip+1] << 48)
	| ((u64)vm->bc->bytes.data[vm->ip+2] << 40)
	| ((u64)vm->bc->bytes.data[vm->ip+3] << 32)
	| ((u64)vm->bc->bytes.data[vm->ip+4] << 24)
	| ((u64)vm->bc->bytes.data[vm->ip+5] << 16)
	| ((u64)vm->bc->bytes.data[vm->ip+6] << 8)
	| ((u64)vm->bc->bytes.data[vm->ip+7] << 0);
	vm->ip += 8;
	return res;
}

static u64 calc_bp_pos(gh_vm *vm, int bytes, i64 offset) {
	offset += bytes;
	if (offset > 0)
		if ((u64)offset > vm->bp) fail();
	return vm->bp - offset;
}
// The stack is backwards, so move data from the
// a register to bp-offset-bvtes
static void mov_a_offset8(gh_vm *vm, i64 offset) {
	u64 pos = calc_bp_pos(vm, 1, offset);
	vm->stack.data[pos] = A8;
}

static void mov_a_offset16(gh_vm *vm, i64 offset) {
	u64 pos = calc_bp_pos(vm, 2, offset);
	vm->stack.data[pos+0] = (vm->a >> 8) & 0xff;
	vm->stack.data[pos+1] = (vm->a >> 0) & 0xff;
}

static void mov_a_offset32(gh_vm *vm, i64 offset) {
	u64 pos = calc_bp_pos(vm, 4, offset);
	vm->stack.data[pos+0] = (vm->a >> 24) & 0xff;
	vm->stack.data[pos+1] = (vm->a >> 16) & 0xff;
	vm->stack.data[pos+2] = (vm->a >> 8) & 0xff;
	vm->stack.data[pos+3] = (vm->a >> 0) & 0xff;
}

static void mov_a_offset64(gh_vm *vm, i64 offset) {
	u64 pos = calc_bp_pos(vm, 8, offset);
	vm->stack.data[pos+0] = (vm->a >> 56) & 0xff;
	vm->stack.data[pos+1] = (vm->a >> 48) & 0xff;
	vm->stack.data[pos+2] = (vm->a >> 40) & 0xff;
	vm->stack.data[pos+3] = (vm->a >> 32) & 0xff;
	vm->stack.data[pos+4] = (vm->a >> 24) & 0xff;
	vm->stack.data[pos+5] = (vm->a >> 16) & 0xff;
	vm->stack.data[pos+6] = (vm->a >> 8) & 0xff;
	vm->stack.data[pos+7] = (vm->a >> 0) & 0xff;
}

static void mov_offset_a8(gh_vm *vm, i64 offset) {
	u64 pos = calc_bp_pos(vm, 1, offset);
	vm->a = (u64) vm->stack.data[pos];
}

static void mov_offset_a16(gh_vm *vm, i64 offset) {
	u64 pos = calc_bp_pos(vm, 2, offset);
	vm->a = ((u64)vm->stack.data[pos+0] << 8)
		| ((u64)vm->stack.data[pos+1] << 0);
}

static void mov_offset_a32(gh_vm *vm, i64 offset) {
	u64 pos = calc_bp_pos(vm, 4, offset);
	vm->a = ((u64)vm->stack.data[pos+0] << 24)
		| ((u64)vm->stack.data[pos+1] << 16)
		| ((u64)vm->stack.data[pos+2] << 8)
		| ((u64)vm->stack.data[pos+3] << 0);
}

static void mov_offset_a64(gh_vm *vm, i64 offset) {
	u64 pos = calc_bp_pos(vm, 8, offset);
	vm->a = ((u64)vm->stack.data[pos+0] << 56)
		| ((u64)vm->stack.data[pos+1] << 48)
		| ((u64)vm->stack.data[pos+2] << 40)
		| ((u64)vm->stack.data[pos+3] << 32)
		| ((u64)vm->stack.data[pos+4] << 24)
		| ((u64)vm->stack.data[pos+5] << 16)
		| ((u64)vm->stack.data[pos+6] << 8)
		| ((u64)vm->stack.data[pos+7] << 0);
}

static void sign_a8(gh_vm *vm) { vm->a |= 0x80; }
static void sign_a16(gh_vm *vm) { vm->a |= 0x8000; }
static void sign_a32(gh_vm *vm) { vm->a |= 0x80000000; }
static void sign_a64(gh_vm *vm) { vm->a |= 0x8000000000000000; }

static void zext_a8_16(gh_vm *vm) { vm->a = A8; }
static void zext_a16_32(gh_vm *vm) { vm->a = A16; }
static void zext_a32_64(gh_vm *vm) { vm->a = A32; }

static void sext_a8_16(gh_vm *vm) {
	i16 i = (i16) ((i8) A8);
	vm->a = (u64) i;
}

static void sext_a16_32(gh_vm *vm) {
	i32 i = (i32) ((i16) A16);
	vm->a = (u64) i;
}

static void sext_a32_64(gh_vm *vm) {
	i32 i = (i64) ((i32) A32);
	vm->a = (u64) i;
}

static void mov_imm_a8(gh_vm *vm) { vm->a = (u64) get8(vm); }
static void mov_imm_a16(gh_vm *vm) { vm->a = (u64) get16(vm); }
static void mov_imm_a32(gh_vm *vm) { vm->a = (u64) get32(vm); }
static void mov_imm_a64(gh_vm *vm) { vm->a = (u64) get64(vm); }

static void neg_a8(gh_vm *vm) { vm->a = !A8; }
static void neg_a16(gh_vm *vm) { vm->a = !A16; }
static void neg_a32(gh_vm *vm) { vm->a = !A32; }
static void neg_a64(gh_vm *vm) { vm->a = !A64; }

static void bneg_a8(gh_vm *vm) { vm->a = (u64) ~A8; }
static void bneg_a16(gh_vm *vm) { vm->a = (u64) ~A16; }
static void bneg_a32(gh_vm *vm) { vm->a = (u64) ~A32; }
static void bneg_a64(gh_vm *vm) { vm->a = (u64) ~A64; }

static void push_val8(gh_vm *vm, u8 b) {
	GROW_VEC(vm->stack, 1);
	APPEND_VEC_RAW(vm->stack, b);
}

static void push_val16(gh_vm *vm, u16 w) {
	GROW_VEC(vm->stack, 2);
	APPEND_VEC_RAW(vm->stack, (u8) (w << 8 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (w << 0 & 0xff));
}

static void push_val32(gh_vm *vm, u32 dw) {
	GROW_VEC(vm->stack, 4);
	APPEND_VEC_RAW(vm->stack, (u8) (dw << 24 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (dw << 16 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (dw << 8 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (dw << 0 & 0xff));
}

static void push_val64(gh_vm *vm, u64 qw) {
	GROW_VEC(vm->stack, 8);
	APPEND_VEC_RAW(vm->stack, (u8) (qw << 56 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (qw << 48 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (qw << 40 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (qw << 32 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (qw << 24 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (qw << 16 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (qw << 8 & 0xff));
	APPEND_VEC_RAW(vm->stack, (u8) (qw << 0 & 0xff));
}

static u8 pop_val8(gh_vm *vm) {
	return vm->stack.data[--SP];
}

static u16 pop_val16(gh_vm *vm) {
	u16 res = ((u16)vm->stack.data[SP-2] << 8)
	| ((u16)vm->stack.data[SP-1] << 0);
	SP -= 2;
	return res;
}

static u32 pop_val32(gh_vm *vm) {
	u32 res = ((u32)vm->stack.data[SP-4] << 24)
	| ((u32)vm->stack.data[SP-3] << 16)
	| ((u32)vm->stack.data[SP-2] << 8)
	| ((u32)vm->stack.data[SP-1] << 0);
	SP -= 4;
	return res;
}

static u64 pop_val64(gh_vm *vm) {
	u64 res = ((u64)vm->stack.data[SP-8] << 56)
	| ((u64)vm->stack.data[SP-7] << 48)
	| ((u64)vm->stack.data[SP-6] << 40)
	| ((u64)vm->stack.data[SP-5] << 32)
	| ((u64)vm->stack.data[SP-4] << 24)
	| ((u64)vm->stack.data[SP-3] << 16)
	| ((u64)vm->stack.data[SP-2] << 8)
	| ((u64)vm->stack.data[SP-1] << 0);
	SP -= 8;
	return res;
}

static void enter(gh_vm *vm) {
	push_val64(vm, vm->bp);
	vm->bp = SP;
}

static void leave(gh_vm *vm) {
	SP = vm->bp;
	vm->bp = pop_val64(vm);
}

static void add_sp(gh_vm *vm) {
	i64 offset = (i64)get64(vm);
	if (offset > 0)
		if ((u64)offset > SP) fail();
	SP -= offset;
}

static void push8(gh_vm *vm) { push_val8(vm, A8); }
static void push16(gh_vm *vm) { push_val16(vm, A16); }
static void push32(gh_vm *vm) { push_val32(vm, A32); }
static void push64(gh_vm *vm) { push_val64(vm, A64); }

#define OPFUNS(name, op) \
static void name ## 8 (gh_vm *vm) { vm->a = pop_val8(vm) op A8; } \
static void name ## 16 (gh_vm *vm) { vm->a = pop_val16(vm) op A16; } \
static void name ## 32 (gh_vm *vm) { vm->a = pop_val32(vm) op A32; } \
static void name ## 64 (gh_vm *vm) { vm->a = pop_val64(vm) op A64; }

OPFUNS(add, +) ;
OPFUNS(sub, -) ;
OPFUNS(mul, *) ;
OPFUNS(div, /) ;
OPFUNS(mod, %) ;
OPFUNS(lshift, <<) ;
OPFUNS(rshift, >>) ;
OPFUNS(band, &) ;
OPFUNS(bxor, ^) ;
OPFUNS(bor, |) ;
OPFUNS(and, &&) ;
OPFUNS(or, ||) ;

#define CMP_FUN(bits) \
static void cmp ## bits(gh_vm *vm) { \
	u ## bits b = pop_val ## bits(vm); \
	vm->f_gt = b > A ## bits; \
	vm->f_eql = b == A ## bits; \
	vm->f_lt = b < A ## bits; \
}

CMP_FUN(8) ; CMP_FUN(16) ; CMP_FUN(32) ; CMP_FUN(64) ;

static void setlt(gh_vm *vm) { vm->a = (u64) vm->f_lt; }
static void setgt(gh_vm *vm) { vm->a = (u64) vm->f_gt; }
static void setle(gh_vm *vm) { vm->a = (u64) (vm->f_lt||vm->f_eql); }
static void setge(gh_vm *vm) { vm->a = (u64) (vm->f_gt||vm->f_eql); }
static void seteq(gh_vm *vm) { vm->a = (u64) vm->f_eql; }
static void setneq(gh_vm *vm) { vm->a = (u64) !vm->f_eql; }

#define JZ_FUN(bits) \
static void jz ## bits(gh_vm *vm) { \
	u64 addr = get64(vm); \
	if (!A ## bits) vm->ip = addr; \
}

JZ_FUN(8) ; JZ_FUN(16) ; JZ_FUN(32) ; JZ_FUN(64) ;

static void jmp(gh_vm *vm) { vm->ip = get64(vm); }

static void call(gh_vm *vm) {
	u64 addr = get64(vm);
	push_val64(vm, vm->ip);
	vm->ip = addr;
}

static void sysfun_print8(gh_vm *vm) {
	enter(vm);
	mov_offset_a8(vm, 8);
	printf("%d\n", (int)A8);
	leave(vm);
}

static void sysfun_print16(gh_vm *vm) {
	enter(vm);
	mov_offset_a16(vm, 8);
	printf("%hd\n", (short)A16);
	leave(vm);
}

static void sysfun_print32(gh_vm *vm) {
	enter(vm);
	mov_offset_a32(vm, 8);
	printf("%d\n", (int)A32);
	leave(vm);
}

static void sysfun_print64(gh_vm *vm) {
	enter(vm);
	mov_offset_a64(vm, 8);
	printf("%lld\n", (long long)A64);
	leave(vm);
}

static void (*sysfuns[])(gh_vm *vm) = {
	[GH_SYSFUN_PRINT8] = sysfun_print8,
	[GH_SYSFUN_PRINT16] = sysfun_print16,
	[GH_SYSFUN_PRINT32] = sysfun_print32,
	[GH_SYSFUN_PRINT64] = sysfun_print64,
};

static void sysfun(gh_vm *vm) {
	u64 idx = get64(vm);
	if (idx > GH_SYSFUN_LAST) fail();
	sysfuns[idx](vm);
}

static void ret(gh_vm *vm) {
	vm->ip = pop_val64(vm);
}

void gh_vm_run(gh_vm *vm) {
	if (setjmp(fail_buf))
		return ;

	if (!vm->bc->main_defined)
		fail();

	vm->ip = vm->bc->funs.data[vm->bc->main_idx].offset;
	while (vm->ip < vm->bc->bytes.used) {
		//gh_log(GH_LOG_INFO, "ip: 0x%llx, a: %lld", vm->ip, vm->a);
		u8 op = vm->bc->bytes.data[vm->ip++];
		switch (op) {
			case GH_VM_MOV_A_OFFSET8:  mov_a_offset8(vm, (i64)get64(vm)); break;
			case GH_VM_MOV_A_OFFSET16: mov_a_offset16(vm, (i64)get64(vm)); break;
			case GH_VM_MOV_A_OFFSET32: mov_a_offset32(vm, (i64)get64(vm)); break;
			case GH_VM_MOV_A_OFFSET64: mov_a_offset64(vm, (i64)get64(vm)); break;

			case GH_VM_MOV_OFFSET_A8:  mov_offset_a8(vm, (i64)get64(vm)); break;
			case GH_VM_MOV_OFFSET_A16: mov_offset_a16(vm, (i64)get64(vm)); break;
			case GH_VM_MOV_OFFSET_A32: mov_offset_a32(vm, (i64)get64(vm)); break;
			case GH_VM_MOV_OFFSET_A64: mov_offset_a64(vm, (i64)get64(vm)); break;

			case GH_VM_SIGN_A8: sign_a8(vm); break;
			case GH_VM_SIGN_A16: sign_a16(vm); break;
			case GH_VM_SIGN_A32: sign_a32(vm); break;
			case GH_VM_SIGN_A64: sign_a64(vm); break;

			case GH_VM_ZEXT_A8_16: zext_a8_16(vm); break;
			case GH_VM_ZEXT_A16_32: zext_a16_32(vm); break;
			case GH_VM_ZEXT_A32_64: zext_a32_64(vm); break;

			case GH_VM_SEXT_A8_16: sext_a8_16(vm); break;
			case GH_VM_SEXT_A16_32: sext_a16_32(vm); break;
			case GH_VM_SEXT_A32_64: sext_a32_64(vm); break;

			case GH_VM_MOV_IMM_A8: mov_imm_a8(vm); break;
			case GH_VM_MOV_IMM_A16: mov_imm_a16(vm); break;
			case GH_VM_MOV_IMM_A32: mov_imm_a32(vm); break;
			case GH_VM_MOV_IMM_A64: mov_imm_a64(vm); break;

			case GH_VM_NEG_A8: neg_a8(vm); break;
			case GH_VM_NEG_A16: neg_a16(vm); break;
			case GH_VM_NEG_A32: neg_a32(vm); break;
			case GH_VM_NEG_A64: neg_a64(vm); break;

			case GH_VM_BNEG_A8: bneg_a8(vm); break;
			case GH_VM_BNEG_A16: bneg_a16(vm); break;
			case GH_VM_BNEG_A32: bneg_a32(vm); break;
			case GH_VM_BNEG_A64: bneg_a64(vm); break;

			case GH_VM_ENTER: enter(vm); break;

			case GH_VM_LEAVE: leave(vm); break;

			case GH_VM_ADD_SP: add_sp(vm); break;

			case GH_VM_PUSH8: push8(vm); break;
			case GH_VM_PUSH16: push16(vm); break;
			case GH_VM_PUSH32: push32(vm); break;
			case GH_VM_PUSH64: push64(vm); break;

			case GH_VM_ADD8: add8(vm); break;
			case GH_VM_ADD16: add16(vm); break;
			case GH_VM_ADD32: add32(vm); break;
			case GH_VM_ADD64: add64(vm); break;

			case GH_VM_SUB8: sub8(vm); break;
			case GH_VM_SUB16: sub16(vm); break;
			case GH_VM_SUB32: sub32(vm); break;
			case GH_VM_SUB64: sub64(vm); break;

			case GH_VM_MUL8: mul8(vm); break;
			case GH_VM_MUL16: mul16(vm); break;
			case GH_VM_MUL32: mul32(vm); break;
			case GH_VM_MUL64: mul64(vm); break;

			case GH_VM_DIV8: div8(vm); break;
			case GH_VM_DIV16: div16(vm); break;
			case GH_VM_DIV32: div32(vm); break;
			case GH_VM_DIV64: div64(vm); break;

			case GH_VM_MOD8: mod8(vm); break;
			case GH_VM_MOD16: mod16(vm); break;
			case GH_VM_MOD32: mod32(vm); break;
			case GH_VM_MOD64: mod64(vm); break;

			case GH_VM_LSHIFT8: lshift8(vm); break;
			case GH_VM_LSHIFT16: lshift16(vm); break;
			case GH_VM_LSHIFT32: lshift32(vm); break;
			case GH_VM_LSHIFT64: lshift64(vm); break;

			case GH_VM_RSHIFT8: rshift8(vm); break;
			case GH_VM_RSHIFT16: rshift16(vm); break;
			case GH_VM_RSHIFT32: rshift32(vm); break;
			case GH_VM_RSHIFT64: rshift64(vm); break;

			case GH_VM_CMP8: cmp8(vm); break;
			case GH_VM_CMP16: cmp16(vm); break;
			case GH_VM_CMP32: cmp32(vm); break;
			case GH_VM_CMP64: cmp64(vm); break;

			case GH_VM_SETLT: setlt(vm); break;
			case GH_VM_SETGT: setgt(vm); break;
			case GH_VM_SETLE: setle(vm); break;
			case GH_VM_SETGE: setge(vm); break;
			case GH_VM_SETEQ: seteq(vm); break;
			case GH_VM_SETNEQ: setneq(vm); break;

			case GH_VM_BAND8: band8(vm); break;
			case GH_VM_BAND16: band16(vm); break;
			case GH_VM_BAND32: band32(vm); break;
			case GH_VM_BAND64: band64(vm); break;

			case GH_VM_BXOR8: bxor8(vm); break;
			case GH_VM_BXOR16: bxor16(vm); break;
			case GH_VM_BXOR32: bxor32(vm); break;
			case GH_VM_BXOR64: bxor64(vm); break;

			case GH_VM_BOR8: bor8(vm); break;
			case GH_VM_BOR16: bor16(vm); break;
			case GH_VM_BOR32: bor32(vm); break;
			case GH_VM_BOR64: bor64(vm); break;

			case GH_VM_AND8: and8(vm); break;
			case GH_VM_AND16: and16(vm); break;
			case GH_VM_AND32: and32(vm); break;
			case GH_VM_AND64: and64(vm); break;

			case GH_VM_OR8: or8(vm); break;
			case GH_VM_OR16: or16(vm); break;
			case GH_VM_OR32: or32(vm); break;
			case GH_VM_OR64: or64(vm); break;

			case GH_VM_JZ8: jz8(vm); break;
			case GH_VM_JZ16: jz16(vm); break;
			case GH_VM_JZ32: jz32(vm); break;
			case GH_VM_JZ64: jz64(vm); break;

			case GH_VM_JMP: jmp(vm); break;

			case GH_VM_CALL: call(vm); break;

			case GH_VM_RET: ret(vm); break;
			case GH_VM_SYSFUN: sysfun(vm); break;
			case GH_VM_EXIT: goto end;
		}
	}

end:
}

void gh_vm_debug(FILE *fp, gh_vm *vm) {
	(void) fp; (void) vm;
}

void gh_vm_deinit(gh_vm *vm) {
	FREE_VEC(vm->stack);
}
