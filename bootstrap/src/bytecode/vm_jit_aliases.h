#ifndef MMAY_BYTECODE_VM_JIT_ALIASES_H
#define MMAY_BYTECODE_VM_JIT_ALIASES_H

#include "mmay/bytecode/function.h"

/* Emission-only exact-reference equivalence. Storage has one entry per verified
 * register; no runtime pointer or managed value is retained by this owner. */
typedef struct MarimayBytecodeJitAliases {
    const MarimayBytecodeFunction *function;
    uint32_t *sources;
} MarimayBytecodeJitAliases;

void mmay_bytecode_jit_aliases_init(MarimayBytecodeJitAliases *aliases,
    const MarimayBytecodeFunction *function, uint32_t *storage);
/* Required at every independent native entry and call/return boundary. */
void mmay_bytecode_jit_aliases_reset(MarimayBytecodeJitAliases *aliases);
uint32_t mmay_bytecode_jit_alias_source(const MarimayBytecodeJitAliases *aliases,
    uint32_t reg);
/* A write destroys every relation that used this register as its source. */
void mmay_bytecode_jit_alias_write(MarimayBytecodeJitAliases *aliases, uint32_t reg);
void mmay_bytecode_jit_alias_copy(MarimayBytecodeJitAliases *aliases,
    uint32_t destination, uint32_t source);

#endif
