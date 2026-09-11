#ifndef MMAY_BYTECODE_VM_HOT_H
#define MMAY_BYTECODE_VM_HOT_H

#include "mmay/bytecode/vm.h"

/* D104: verified, rooted program frame only. No allocation or safepoint;
 * stops before an unsupported/failing instruction without consuming it.
 * Cursor pointers name distinct fields of that frame, separate from registers
 * and fuel. The cursor is published at every exit, before any observer runs. */
void mmay_bytecode_run_hot(const MarimayBytecodeFunction *function,
    MarimayBytecodeValue *registers, MarimayBytecodeExecution *execution,
    size_t *block, size_t *instruction, size_t *steps,
    MarimayBytecodeRunOptions options);

/* Borrow only a complete existing descriptor; never creates one. */
const MarimayManagedArrayDescriptor *mmay_bytecode_array_descriptor_cached(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    MarimayBytecodeType type, uint32_t layout);

const MarimayManagedRecordDescriptor *mmay_bytecode_record_descriptor_cached(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    uint32_t layout);

#ifdef MMAY_HAVE_SLJIT
const MarimayManagedEnumDescriptor *mmay_bytecode_enum_descriptor_cached(
    MarimayBytecodeExecution *execution, const MarimayBytecodeFunction *function,
    uint32_t layout);
#endif

#endif
