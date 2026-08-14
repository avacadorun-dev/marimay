#ifndef RUDA_TEST_BYTECODE_VM_SUPPORT_H
#define RUDA_TEST_BYTECODE_VM_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/vm.h"

RudaBytecodeInstruction ruda_test_vm_instruction(RudaOpcode opcode, uint64_t first,
    uint64_t second, uint64_t third, uint64_t span_start);
bool ruda_test_vm_append(RudaBytecodeFunction *function, uint32_t block,
    RudaOpcode opcode, uint64_t first, uint64_t second, uint64_t third,
    uint64_t *span_start);
RudaBytecodeInstruction *ruda_test_vm_at(RudaBytecodeFunction *function,
    size_t block, size_t instruction);
bool ruda_test_vm_add_registers(RudaBytecodeFunction *function,
    const RudaBytecodeType *types, size_t count);
bool ruda_test_vm_build_all_opcodes(RudaBytecodeFunction *function, RudaAllocator allocator);
bool ruda_test_vm_build_binary(RudaBytecodeFunction *function, RudaAllocator allocator,
    RudaOpcode opcode);
bool ruda_test_vm_build_i64_comparison(RudaBytecodeFunction *function,
    RudaAllocator allocator, RudaOpcode opcode);
bool ruda_test_vm_build_bool_operation(RudaBytecodeFunction *function,
    RudaAllocator allocator, RudaOpcode opcode);
bool ruda_test_vm_build_constants_and_copy(RudaBytecodeFunction *function,
    RudaAllocator allocator);
bool ruda_test_vm_build_bool_constant_copy(RudaBytecodeFunction *function,
    RudaAllocator allocator, bool value);
bool ruda_test_vm_build_loop(RudaBytecodeFunction *function, RudaAllocator allocator);
bool ruda_test_vm_build_void(RudaBytecodeFunction *function, RudaAllocator allocator);
bool ruda_test_vm_build_direct_constant(RudaBytecodeFunction *function,
    RudaAllocator allocator, int64_t value, uint64_t span_start);
bool ruda_test_vm_build_direct_forward(RudaBytecodeFunction *function,
    RudaAllocator allocator, RudaSymbolId callee, uint64_t span_start);
bool ruda_test_vm_build_nested_program(RudaBytecodeProgram *program, RudaAllocator allocator);
bool ruda_test_vm_build_recursive_program(RudaBytecodeProgram *program,
    RudaAllocator allocator);
bool ruda_test_vm_build_negate(RudaBytecodeFunction *function, RudaAllocator allocator);
RudaBytecodeValue ruda_test_vm_bool(bool value);
RudaBytecodeValue ruda_test_vm_i64(int64_t value);
bool ruda_test_vm_build_f64(RudaBytecodeFunction *function, RudaOpcode opcode,
    uint64_t left_bits, uint64_t right_bits, RudaBytecodeType result_type);
bool ruda_test_vm_build_string_literal(RudaBytecodeFunction *function,
    RudaAllocator allocator, const unsigned char *bytes, size_t length,
    bool panic);
bool ruda_test_vm_build_string_equal(RudaBytecodeFunction *function,
    RudaAllocator allocator, const unsigned char *left, size_t left_length,
    const unsigned char *right, size_t right_length);
bool ruda_test_vm_build_string_identity(RudaBytecodeFunction *function,
    RudaAllocator allocator);

void ruda_test_vm_all_opcodes_and_limits(void);
void ruda_test_vm_checked_arithmetic(void);
void ruda_test_vm_scalar_results(void);
void ruda_test_vm_bitwise_and_shifts(void);
void ruda_test_vm_f64(void);
void ruda_test_vm_strings_and_panic(void);
void ruda_test_vm_direct_calls(void);
void ruda_test_vm_managed_arrays(void);
void ruda_test_vm_array_feature_call_metadata(void);
void ruda_test_vm_checked_index_cfg(void);
void ruda_test_vm_array_rhs_ordering(void);
void ruda_test_vm_managed_array_calls(void);
void ruda_test_vm_managed_array_provenance(void);
void ruda_test_vm_managed_array_panic_frames(void);
void ruda_test_vm_interfaces(void);
void ruda_test_vm_interface_values(void);
void ruda_test_vm_nominal_values_and_verifier(void);
void ruda_test_vm_nominal_host_boundary(void);
void ruda_test_vm_nominal_identity_and_strings(void);
void ruda_test_vm_nominal_codec(void);
void ruda_test_vm_rejection_round_trip_and_allocation(void);

#endif
