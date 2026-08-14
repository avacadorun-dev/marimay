#ifndef RUDA_TEST_BYTECODE_SUPPORT_H
#define RUDA_TEST_BYTECODE_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/program.h"

typedef struct RudaTestOpcodeExpectation {
    RudaOpcode opcode;
    uint8_t wire_id;
    const char *mnemonic;
    uint8_t operand_count;
    RudaBytecodeOperandKind operands[3];
    uint32_t flags;
} RudaTestOpcodeExpectation;

RudaBytecodeInstruction ruda_test_bytecode_instruction(RudaOpcode opcode,
    uint64_t first, uint64_t second, uint64_t third, size_t span_index);
bool ruda_test_bytecode_append(RudaBytecodeFunction *function, uint32_t block,
    RudaOpcode opcode, uint64_t first, uint64_t second, uint64_t third,
    size_t *span_index);
bool ruda_test_bytecode_read_u64(const unsigned char *bytes, size_t length,
    size_t *offset, uint64_t *result);
bool ruda_test_bytecode_build_full(RudaBytecodeFunction *function, RudaAllocator allocator);
bool ruda_test_bytecode_build_void(RudaBytecodeFunction *function, RudaAllocator allocator);
bool ruda_test_bytecode_build_constant(RudaBytecodeFunction *function,
    RudaAllocator allocator, int64_t value);
bool ruda_test_bytecode_build_direct_callee(RudaBytecodeFunction *function,
    RudaAllocator allocator);
bool ruda_test_bytecode_build_direct_caller(RudaBytecodeFunction *function,
    RudaAllocator allocator, RudaSymbolId callee);
bool ruda_test_bytecode_build_string_constant(RudaBytecodeFunction *function,
    RudaAllocator allocator, const unsigned char *bytes, size_t length);
bool ruda_test_bytecode_build_string_equality(RudaBytecodeFunction *function,
    RudaAllocator allocator, bool assign_second);
bool ruda_test_bytecode_build_panic_string(RudaBytecodeFunction *function,
    RudaAllocator allocator, RudaBytecodeType type, bool assign, bool nonterminal);
bool ruda_test_bytecode_build_direct_program(RudaBytecodeProgram *program,
    RudaAllocator allocator);
RudaBytecodeInstruction *ruda_test_bytecode_at(RudaBytecodeFunction *function,
    size_t block, size_t instruction);

void ruda_test_bytecode_metadata(void);
void ruda_test_bytecode_valid_and_disassembly(void);
void ruda_test_bytecode_codec(void);
void ruda_test_bytecode_f64_codec_and_verifier(void);
void ruda_test_bytecode_decode_rejections(void);
void ruda_test_bytecode_verifier_rejections(void);
void ruda_test_bytecode_calls_and_program(void);
void ruda_test_bytecode_string_pool(void);
void ruda_test_bytecode_string_verifier_rejections(void);
void ruda_test_bytecode_allocation_failures(void);
void ruda_test_bytecode_verifier_provenance(void);
void ruda_test_bytecode_nominal_verifier_matrix(void);
void ruda_test_bytecode_nominal_program_schemas(void);

#endif
