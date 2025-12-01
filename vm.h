#ifndef _GALACH_VM_H
#define _GALACH_VM_H

#include <stdio.h>
#include "bytecode.h"

// NOTE: For some of these instructions, *the order is important*
// It must be 8, 16, 32, 64 for each instruction, where applicable.
typedef enum {
	// Move data from the a register to bp+offset
	// bits: |  8  |  64  |
	//       ^     ^- offset
	//       ^- op
	GH_VM_MOV_A_OFFSET8,
	GH_VM_MOV_A_OFFSET16,
	GH_VM_MOV_A_OFFSET32,
	GH_VM_MOV_A_OFFSET64,

	// Move data from bp+offset to the a register
	// bits: |  8  |  64  |
	//       ^     ^- offset
	//       ^- op
	GH_VM_MOV_OFFSET_A8,
	GH_VM_MOV_OFFSET_A16,
	GH_VM_MOV_OFFSET_A32,
	GH_VM_MOV_OFFSET_A64,

	// Switch the sign of the a register
	// bits: |  8  |
	//       ^- op
	GH_VM_SIGN_A8,
	GH_VM_SIGN_A16,
	GH_VM_SIGN_A32,
	GH_VM_SIGN_A64,

	// Zero-extends the value in the a register to a larger bitwidth
	// bits: |  8  |
	//       ^- op
	GH_VM_ZEXT_A8_16,
	GH_VM_ZEXT_A16_32,
	GH_VM_ZEXT_A32_64,

	// Sign-extends the value in the a register to a larger bitwidth
	// bits: |  8  |
	//       ^- op
	GH_VM_SEXT_A8_16,
	GH_VM_SEXT_A16_32,
	GH_VM_SEXT_A32_64,

	// Move an immediate into the a register
	// bits: |  8  |  8/16/32/64  |
	//       ^     ^- data
	//       ^- op
	// The a register is 64 bits, so any data provided that's
	// under that will go into the lower bits of a.
	GH_VM_MOV_IMM_A8,
	GH_VM_MOV_IMM_A16,
	GH_VM_MOV_IMM_A32,
	GH_VM_MOV_IMM_A64,

	// If a is nonzero, make it zero. Otherwise, set it to one.
	// bits: |  8  |
	//       ^- op
	GH_VM_NEG_A8,
	GH_VM_NEG_A16,
	GH_VM_NEG_A32,
	GH_VM_NEG_A64,

	// Do a bitwise negate of the bits in a.
	// bits: |  8  |
	//       ^_ op
	GH_VM_BNEG_A8,
	GH_VM_BNEG_A16,
	GH_VM_BNEG_A32,
	GH_VM_BNEG_A64,

	// Create a new stack frame
	// bits: |  8  |
	//       ^- op
	GH_VM_ENTER,

	// Leave the stack frame
	// bits: |  8  |
	//       ^- op
	GH_VM_LEAVE,

	// Add an offset to the stack pointer
	// bits: |  8  |  64  |
	//       ^     ^- offset
	//       ^- op
	GH_VM_ADD_SP,

	// Push n bits from the a register onto the stack
	// bits: |  8  |
	//       ^- op
	GH_VM_PUSH8,
	GH_VM_PUSH16,
	GH_VM_PUSH32,
	GH_VM_PUSH64,

	// === Integer arithmetic ===
	// Pops a value from the stack and adds it to the a register
	// bits: |  8  |
	//       ^- op
	GH_VM_ADD8,
	GH_VM_ADD16,
	GH_VM_ADD32,
	GH_VM_ADD64,

	// Pops a value from the stack and subtracts the a register from this value
	// bits: |  8  |
	//       ^- op
	GH_VM_SUB8,
	GH_VM_SUB16,
	GH_VM_SUB32,
	GH_VM_SUB64,

	// Pops a value from the stack and multiplies it with the a register
	// bits: |  8  |
	//       ^- op
	GH_VM_MUL8,
	GH_VM_MUL16,
	GH_VM_MUL32,
	GH_VM_MUL64,

	// Pops a value from the stack and divides the a register from this value
	// bits: |  8  |
	//       ^- op
	GH_VM_DIV8,
	GH_VM_DIV16,
	GH_VM_DIV32,
	GH_VM_DIV64,

	// === Bit shifting ===
	// Shifts a value from the stack to the left by the 8-bit value in the a register
	// bits: |  8  |
	//       ^- op
	// important: a = val << a
	GH_VM_LSHIFT8,
	GH_VM_LSHIFT16,
	GH_VM_LSHIFT32,
	GH_VM_LSHIFT64,

	// Shifts a value from the stack to the left by the 8-bit value in the a register
	// bits: |  8  |
	//       ^- op
	// important: a = val >> a
	GH_VM_RSHIFT8,
	GH_VM_RSHIFT16,
	GH_VM_RSHIFT32,
	GH_VM_RSHIFT64,

	// === Logical arithmetic ===
	// Compares the value on the stack with the value in the a register.
	// The stack value is the "first" value, and the one in a is the "second" value.
	// bits: |  8  |
	//       ^- op
	GH_VM_CMP8,
	GH_VM_CMP16,
	GH_VM_CMP32,
	GH_VM_CMP64,

	// Sets the a register depending on the flags:
	// LT = less than
	// GT = greater than
	// LE = less than or equal
	// GE = greater than or equal
	// EQ = equal
	// NEQ = not equal
	// Follows after the CMP instruction
	// bits: |  8  |
	//       ^- op
	GH_VM_SETLT,
	GH_VM_SETGT,
	GH_VM_SETLE,
	GH_VM_SETGE,
	GH_VM_SETEQ,
	GH_VM_SETNEQ,

	// Pops a value from the stack and does a bitwise and with the a register
	// bits: |  8  |
	//       ^- op
	GH_VM_BAND8,
	GH_VM_BAND16,
	GH_VM_BAND32,
	GH_VM_BAND64,

	// Pops a value from the stack and does a bitwise xor with the a register
	// bits: |  8  |
	//       ^- op
	GH_VM_BXOR8,
	GH_VM_BXOR16,
	GH_VM_BXOR32,
	GH_VM_BXOR64,

	// Pops a value from the stack and does a bitwise or with the a register
	// bits: |  8  |
	//       ^- op
	GH_VM_BOR8,
	GH_VM_BOR16,
	GH_VM_BOR32,
	GH_VM_BOR64,

	// If both the stack value and a are non zero, set a to one, else zero.
	// bits: |  8  |
	//       ^- op
	GH_VM_AND8,
	GH_VM_AND16,
	GH_VM_AND32,
	GH_VM_AND64,

	// If either the stack value and a are non zero, set a to one, else zero.
	// bits: |  8  |
	//       ^- op
	GH_VM_OR8,
	GH_VM_OR16,
	GH_VM_OR32,
	GH_VM_OR64,

	// If the a register is zero, jump to this address
	// bits: |  8  |  64  |
	//       ^     ^- address
	//       ^- op
	GH_VM_JZ8,
	GH_VM_JZ16,
	GH_VM_JZ32,
	GH_VM_JZ64,

	// Jump to this address
	// bits: |  8  |  64  |
	//       ^     ^- address
	//       ^- op
	GH_VM_JMP,

	// Return from the function
	GH_VM_RET,
} gh_vm_op;

typedef struct {
	gh_bytecode *bc;
	struct {
		u8 *b;
		u64 used;
		u64 size;
	} stack;
	u64 ip;
	u64 sp;
	u64 bp;
	u64 a;

	// Equal flag
	// If the CMP between the two values shows they are identical,
	// this is turned on.
	u8 f_eql : 1;

	// Greater flag
	// If the CMP between the two values shows that the first one
	// is greater than the second, this is set.
	u8 f_gt  : 1;

	// Less flag
	// If the CMP between the two value shows that the first one
	// is less than the second, this is set.
	u8 f_lt  : 1;
} gh_vm;

void gh_vm_init(gh_vm *vm, gh_bytecode *bytecode);
void gh_vm_run(gh_vm *vm);

void gh_vm_debug(FILE *fp, gh_vm *vm);
void gh_vm_deinit(gh_vm *vm);


#endif // _GALACH_VM_H
