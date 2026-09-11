#include "vm_jit_aliases.h"
#include "bytecode_internal.h"

void mmay_bytecode_jit_aliases_reset(MarimayBytecodeJitAliases *aliases) {
    for (size_t reg = 0; reg < aliases->function->register_types.length; ++reg)
        aliases->sources[reg] = (uint32_t)reg;
}

void mmay_bytecode_jit_aliases_init(MarimayBytecodeJitAliases *aliases,
    const MarimayBytecodeFunction *function, uint32_t *storage) {
    *aliases = (MarimayBytecodeJitAliases){function, storage};
    mmay_bytecode_jit_aliases_reset(aliases);
}

uint32_t mmay_bytecode_jit_alias_source(const MarimayBytecodeJitAliases *aliases,
    uint32_t reg) {
    return aliases->sources[reg];
}

void mmay_bytecode_jit_alias_write(MarimayBytecodeJitAliases *aliases, uint32_t reg) {
    /* Relations always point directly at a canonical register. Detach its
     * aliases before it can hold another value or acquire another binding. */
    for (size_t other = 0; other < aliases->function->register_types.length; ++other)
        if (other == reg || aliases->sources[other] == reg)
            aliases->sources[other] = (uint32_t)other;
}

void mmay_bytecode_jit_alias_copy(MarimayBytecodeJitAliases *aliases,
    uint32_t destination, uint32_t source) {
    uint32_t canonical = aliases->sources[source];
    mmay_bytecode_jit_alias_write(aliases, destination);
    const MarimayBytecodeType *types = aliases->function->register_types.data;
    const uint32_t *layouts = aliases->function->register_layouts.data;
    if (types[destination] == types[source]
        && (types[source] == MMAY_BYTECODE_TYPE_RECORD
            || mmay_bytecode_type_is_array(types[source]))
        && layouts[destination] == layouts[source])
        aliases->sources[destination] = canonical;
}
