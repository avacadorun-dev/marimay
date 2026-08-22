#ifndef MMAY_TEST_NATIVE_C_EMITTER_SUPPORT_H
#define MMAY_TEST_NATIVE_C_EMITTER_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/vm.h"
#include "mmay/mir/mir.h"
#include "mmay/native/c_emitter.h"

bool mmay_test_native_contains(
    const MarimayOwnedBytes *bytes,
    const char *needle
);
#if !defined(_WIN32)
bool mmay_test_native_hex_bytes(
    const unsigned char *bytes,
    size_t length,
    char *output,
    size_t capacity
);
#endif
MarimayMirInstruction mmay_test_native_instruction(
    MarimayMirInstructionKind kind,
    MarimayMirRegister target,
    MarimayMirRegister first,
    MarimayMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start
);
MarimayMirTerminator mmay_test_native_terminator(
    MarimayMirTerminatorKind kind,
    MarimayMirRegister value,
    MarimayMirBlockId target,
    MarimayMirBlockId otherwise,
    size_t start
);
#if !defined(_WIN32)
void mmay_test_native_compile_and_run_configured(
    const MarimayOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit,
    const char *configuration
);
void mmay_test_native_compile_and_run(
    const MarimayOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit
);
#endif
void mmay_test_native_emit(
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimaySymbolId entry,
    MarimayNativeCEmitOptions options,
    MarimayNativeCEmitStatus expected
);
void mmay_test_native_vm_program(
    const MarimayMirFunction *const *functions,
    MarimaySymbolId entry_symbol,
    int64_t expected
);
void mmay_test_native_vm_overflow(const MarimayMirFunction *input);
bool mmay_test_native_build_panic_leaf(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
);

#endif
