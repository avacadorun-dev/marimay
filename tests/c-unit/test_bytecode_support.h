#ifndef MMAY_TEST_BYTECODE_SUPPORT_H
#define MMAY_TEST_BYTECODE_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/bytecode/bytecode.h"
#include "mmay/bytecode/program.h"

typedef struct MarimayTestOpcodeExpectation {
    MarimayOpcode opcode;
    uint8_t wire_id;
    const char *mnemonic;
    uint8_t operand_count;
    MarimayBytecodeOperandKind operands[4];
    uint32_t flags;
} MarimayTestOpcodeExpectation;

MarimayBytecodeInstruction mmay_test_bytecode_instruction(MarimayOpcode opcode,
    uint64_t first, uint64_t second, uint64_t third, size_t span_index);
bool mmay_test_bytecode_append(MarimayBytecodeFunction *function, uint32_t block,
    MarimayOpcode opcode, uint64_t first, uint64_t second, uint64_t third,
    size_t *span_index);
bool mmay_test_bytecode_read_u64(const unsigned char *bytes, size_t length,
    size_t *offset, uint64_t *result);
bool mmay_test_bytecode_build_full(MarimayBytecodeFunction *function, MarimayAllocator allocator);
bool mmay_test_bytecode_build_void(MarimayBytecodeFunction *function, MarimayAllocator allocator);
bool mmay_test_bytecode_build_constant(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, int64_t value);
bool mmay_test_bytecode_build_direct_callee(MarimayBytecodeFunction *function,
    MarimayAllocator allocator);
bool mmay_test_bytecode_build_direct_caller(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, MarimaySymbolId callee);
bool mmay_test_bytecode_build_string_constant(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, const unsigned char *bytes, size_t length);
bool mmay_test_bytecode_build_string_equality(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, bool assign_second);
bool mmay_test_bytecode_build_panic_string(MarimayBytecodeFunction *function,
    MarimayAllocator allocator, MarimayBytecodeType type, bool assign, bool nonterminal);
bool mmay_test_bytecode_build_direct_program(MarimayBytecodeProgram *program,
    MarimayAllocator allocator);
MarimayBytecodeInstruction *mmay_test_bytecode_at(MarimayBytecodeFunction *function,
    size_t block, size_t instruction);

void mmay_test_bytecode_metadata(void);
void mmay_test_bytecode_valid_and_disassembly(void);
void mmay_test_bytecode_codec(void);
void mmay_test_bytecode_f64_codec_and_verifier(void);
void mmay_test_bytecode_decode_rejections(void);
void mmay_test_bytecode_verifier_rejections(void);
void mmay_test_bytecode_integer_widths(void);
void mmay_test_bytecode_calls_and_program(void);
void mmay_test_bytecode_string_pool(void);
void mmay_test_bytecode_string_verifier_rejections(void);
void mmay_test_bytecode_allocation_failures(void);
void mmay_test_bytecode_verifier_provenance(void);
void mmay_test_bytecode_nominal_verifier_matrix(void);
void mmay_test_bytecode_nominal_program_schemas(void);

#endif
