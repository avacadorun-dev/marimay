#ifndef MMAY_TEST_BYTECODE_VM_SUPPORT_H
#define MMAY_TEST_BYTECODE_VM_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/bytecode/bytecode.h"
#include "mmay/bytecode/vm.h"

MarimayBytecodeInstruction mmay_test_vm_instruction(MarimayOpcode opcode, uint64_t first,
    uint64_t second, uint64_t third, uint64_t span_start);
bool mmay_test_vm_append(MarimayBytecodeFunction *function, uint32_t block,
    MarimayOpcode opcode, uint64_t first, uint64_t second, uint64_t third,
    uint64_t *span_start);
MarimayBytecodeInstruction *mmay_test_vm_at(MarimayBytecodeFunction *function,
    size_t block, size_t instruction);
bool mmay_test_vm_add_registers(MarimayBytecodeFunction *function,
    const MarimayBytecodeType *types, size_t count);
bool mmay_test_vm_build_all_opcodes(MarimayBytecodeFunction *function, MarimayAllocator allocator);
bool mmay_test_vm_build_binary(MarimayBytecodeFunction *function, MarimayAllocator allocator,
    MarimayOpcode opcode);
bool mmay_test_vm_build_i64_comparison(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, MarimayOpcode opcode);
bool mmay_test_vm_build_bool_operation(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, MarimayOpcode opcode);
bool mmay_test_vm_build_constants_and_copy(MarimayBytecodeFunction *function,
    MarimayAllocator allocator);
bool mmay_test_vm_build_bool_constant_copy(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, bool value);
bool mmay_test_vm_build_loop(MarimayBytecodeFunction *function, MarimayAllocator allocator);
bool mmay_test_vm_build_void(MarimayBytecodeFunction *function, MarimayAllocator allocator);
bool mmay_test_vm_build_direct_constant(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, int64_t value, uint64_t span_start);
bool mmay_test_vm_build_direct_forward(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, MarimaySymbolId callee, uint64_t span_start);
bool mmay_test_vm_build_nested_program(MarimayBytecodeProgram *program, MarimayAllocator allocator);
bool mmay_test_vm_build_recursive_program(MarimayBytecodeProgram *program,
    MarimayAllocator allocator);
bool mmay_test_vm_build_negate(MarimayBytecodeFunction *function, MarimayAllocator allocator);
MarimayBytecodeValue mmay_test_vm_bool(bool value);
MarimayBytecodeValue mmay_test_vm_i64(int64_t value);
bool mmay_test_vm_build_f64(MarimayBytecodeFunction *function, MarimayOpcode opcode,
    uint64_t left_bits, uint64_t right_bits, MarimayBytecodeType result_type);
bool mmay_test_vm_build_string_literal(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, const unsigned char *bytes, size_t length,
    bool panic);
bool mmay_test_vm_build_string_equal(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, const unsigned char *left, size_t left_length,
    const unsigned char *right, size_t right_length);
bool mmay_test_vm_build_string_identity(MarimayBytecodeFunction *function,
    MarimayAllocator allocator);

void mmay_test_vm_all_opcodes_and_limits(void);
void mmay_test_vm_checked_arithmetic(void);
void mmay_test_vm_scalar_results(void);
void mmay_test_vm_bitwise_and_shifts(void);
void mmay_test_vm_f64(void);
void mmay_test_vm_strings_and_panic(void);
void mmay_test_vm_direct_calls(void);
void mmay_test_vm_managed_arrays(void);
void mmay_test_vm_array_storage_layouts(void);
void mmay_test_vm_array_feature_call_metadata(void);
void mmay_test_vm_checked_index_cfg(void);
void mmay_test_vm_array_rhs_ordering(void);
void mmay_test_vm_array_mutation(void);
void mmay_test_vm_managed_array_calls(void);
void mmay_test_vm_managed_array_provenance(void);
void mmay_test_vm_managed_array_panic_frames(void);
void mmay_test_vm_interfaces(void);
void mmay_test_vm_interface_values(void);
void mmay_test_vm_nominal_values_and_verifier(void);
void mmay_test_vm_nominal_host_boundary(void);
void mmay_test_vm_nominal_identity_and_strings(void);
void mmay_test_vm_nominal_codec(void);
void mmay_test_vm_rejection_round_trip_and_allocation(void);

#endif
