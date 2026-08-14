#ifndef RUDA_TEST_NATIVE_C_EMITTER_SUPPORT_H
#define RUDA_TEST_NATIVE_C_EMITTER_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/vm.h"
#include "ruda/mir/mir.h"
#include "ruda/native/c_emitter.h"

bool ruda_test_native_contains(
    const RudaOwnedBytes *bytes,
    const char *needle
);
#if !defined(_WIN32)
bool ruda_test_native_hex_bytes(
    const unsigned char *bytes,
    size_t length,
    char *output,
    size_t capacity
);
#endif
RudaMirInstruction ruda_test_native_instruction(
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirRegister first,
    RudaMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start
);
RudaMirTerminator ruda_test_native_terminator(
    RudaMirTerminatorKind kind,
    RudaMirRegister value,
    RudaMirBlockId target,
    RudaMirBlockId otherwise,
    size_t start
);
#if !defined(_WIN32)
void ruda_test_native_compile_and_run_configured(
    const RudaOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit,
    const char *configuration
);
void ruda_test_native_compile_and_run(
    const RudaOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit
);
#endif
void ruda_test_native_emit(
    const RudaMirFunction *const *functions,
    size_t count,
    RudaSymbolId entry,
    RudaNativeCEmitOptions options,
    RudaNativeCEmitStatus expected
);
void ruda_test_native_vm_program(
    const RudaMirFunction *const *functions,
    RudaSymbolId entry_symbol,
    int64_t expected
);
void ruda_test_native_vm_overflow(const RudaMirFunction *input);
bool ruda_test_native_build_panic_leaf(
    RudaMirFunction *function,
    RudaSymbolId symbol
);

#endif
