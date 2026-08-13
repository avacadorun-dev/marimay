#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/vm.h"
#include "ruda/check/checker.h"
#include "ruda/lex/lexer.h"
#include "ruda/mir/mir.h"
#include "ruda/native/c_emitter.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"
#if !defined(_WIN32)
#include "ruda/platform/process.h"
#endif

#ifndef RUDA_TEST_C_COMPILER
#define RUDA_TEST_C_COMPILER "cc"
#endif

#ifndef RUDA_TEST_SOURCE_DIR
#define RUDA_TEST_SOURCE_DIR "."
#endif

static bool ruda_test_native_contains(
    const RudaOwnedBytes *bytes,
    const char *needle
) {
    size_t needle_length = needle != NULL ? strlen(needle) : 0;
    size_t index = 0;
    if (bytes == NULL || needle == NULL || needle_length > bytes->length) {
        return false;
    }
    for (index = 0; index + needle_length <= bytes->length; index += 1) {
        if (memcmp(bytes->data + index, needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
}

static RudaMirInstruction ruda_test_native_instruction(
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirRegister first,
    RudaMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    instruction.integer = integer;
    instruction.boolean = boolean;
    instruction.span.start = start;
    instruction.span.end = start + 1;
    return instruction;
}

static RudaMirTerminator ruda_test_native_terminator(
    RudaMirTerminatorKind kind,
    RudaMirRegister value,
    RudaMirBlockId target,
    RudaMirBlockId otherwise,
    size_t start
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = kind;
    terminator.value = value;
    terminator.target = target;
    terminator.otherwise = otherwise;
    terminator.span.start = start;
    terminator.span.end = start + 1;
    return terminator;
}

static bool ruda_test_native_build_leaf(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_I64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &value)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_I64, value,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 42, false, 10);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, value,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 11))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_native_build_entry(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId callee,
    bool branch_value
) {
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId entry = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId yes = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId no = RUDA_MIR_NO_BLOCK;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_I64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &value)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &value)
        || !ruda_mir_add_block(function, &entry)
        || !ruda_mir_add_block(function, &yes)
        || !ruda_mir_add_block(function, &no)
        || !ruda_mir_add_call(function, callee, RUDA_TYPE_I64, NULL, 0, &call)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_BOOL, 1,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, branch_value, 20);
    if (!ruda_mir_append_instruction(function, entry, &instruction)
        || !ruda_mir_set_terminator(function, entry,
            ruda_test_native_terminator(RUDA_MIR_BRANCH, 1, yes, no, 21))) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CALL, 0,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 22);
    instruction.call = call;
    if (!ruda_mir_append_instruction(function, yes, &instruction)
        || !ruda_mir_set_terminator(function, yes,
            ruda_test_native_terminator(RUDA_MIR_RETURN, 0,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 23))) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_I64, 0,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 24);
    if (!ruda_mir_append_instruction(function, no, &instruction)
        || !ruda_mir_set_terminator(function, no,
            ruda_test_native_terminator(RUDA_MIR_RETURN, 0,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 25))) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static bool ruda_test_native_build_overflow(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_I64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &left)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &right)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &result)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_I64, left,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, INT64_MAX, false, 30);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_I64, right,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 1, false, 31);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_ADD_CHECKED_I64, result,
        left, right, 0, false, 32);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 33))) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static bool ruda_test_native_build_array(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    int64_t access_index
) {
    const RudaTypeId array_type = RUDA_TYPE_FIRST_DYNAMIC + 100;
    RudaMirRegister first = RUDA_MIR_NO_REGISTER;
    RudaMirRegister second = RUDA_MIR_NO_REGISTER;
    RudaMirRegister array_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister length = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister checked = RUDA_MIR_NO_REGISTER;
    RudaMirRegister replacement = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId construction = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirArrayArgument arguments[2];
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0,
            RUDA_TYPE_F64, ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &first)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &second)
        || !ruda_mir_add_register(function, array_type, &array_value)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &length)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &index)
        || !ruda_mir_add_register(function, array_type, &checked)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &replacement)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &result)
        || !ruda_mir_add_array(function, array_type, RUDA_TYPE_F64, &array)
        || !ruda_mir_add_block(function, &block)) {
        goto failure;
    }
    arguments[0].register_id = first;
    arguments[0].type = RUDA_TYPE_F64;
    arguments[1].register_id = second;
    arguments[1].type = RUDA_TYPE_F64;
    if (!ruda_mir_add_array_construction(
            function, array, arguments, 2, &construction
        )) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_F64, first, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 800
    );
    instruction.f64_bits = UINT64_C(0x3ff8000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_F64, second, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 801
    );
    instruction.f64_bits = UINT64_C(0x4004000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_NEW, array_value, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 802
    );
    instruction.array = array;
    instruction.array_construction = construction;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_LENGTH, length, array_value, RUDA_MIR_NO_REGISTER,
        0, false, 803
    );
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, index, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, access_index, false, 804
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_REQUIRE_INDEX, checked, array_value, index,
        0, false, 805
    );
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_F64, replacement, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 806
    );
    instruction.f64_bits = UINT64_C(0x4022000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_SET, result, checked, index, 0, false, 805
    );
    instruction.third = replacement;
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_GET, result, array_value, index, 0, false, 807
    );
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block, ruda_test_native_terminator(
                RUDA_MIR_RETURN, result, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 808
            )
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static bool ruda_test_native_build_nested_array(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    const RudaTypeId inner_type = RUDA_TYPE_FIRST_DYNAMIC + 110;
    const RudaTypeId outer_type = RUDA_TYPE_FIRST_DYNAMIC + 111;
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister inner = RUDA_MIR_NO_REGISTER;
    RudaMirRegister outer = RUDA_MIR_NO_REGISTER;
    RudaMirRegister fetched = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirArrayId inner_array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayId outer_array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId inner_construction =
        RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirArrayConstructionId outer_construction =
        RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirArrayArgument argument;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0,
            RUDA_TYPE_F64, ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &value)
        || !ruda_mir_add_register(function, inner_type, &inner)
        || !ruda_mir_add_register(function, outer_type, &outer)
        || !ruda_mir_add_register(function, inner_type, &fetched)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &index)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &result)
        || !ruda_mir_add_array(
            function, inner_type, RUDA_TYPE_F64, &inner_array
        ) || !ruda_mir_add_array(
            function, outer_type, inner_type, &outer_array
        ) || !ruda_mir_add_block(function, &block)) {
        goto failure;
    }
    argument.register_id = value;
    argument.type = RUDA_TYPE_F64;
    if (!ruda_mir_add_array_construction(
            function, inner_array, &argument, 1, &inner_construction
        )) {
        goto failure;
    }
    argument.register_id = inner;
    argument.type = inner_type;
    if (!ruda_mir_add_array_construction(
            function, outer_array, &argument, 1, &outer_construction
        )) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_F64, value, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 820
    );
    instruction.f64_bits = UINT64_C(0x400c000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_NEW, inner, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 821
    );
    instruction.array = inner_array;
    instruction.array_construction = inner_construction;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_NEW, outer, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 822
    );
    instruction.array = outer_array;
    instruction.array_construction = outer_construction;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, index, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 823
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_GET, fetched, outer, index, 0, false, 824
    );
    instruction.array = outer_array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_GET, result, fetched, index, 0, false, 825
    );
    instruction.array = inner_array;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block, ruda_test_native_terminator(
                RUDA_MIR_RETURN, result, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 826
            )
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static bool ruda_test_native_build_array_call(
    RudaMirFunction *leaf,
    RudaMirFunction *entry,
    RudaSymbolId leaf_symbol,
    RudaSymbolId entry_symbol
) {
    const RudaTypeId array_type = RUDA_TYPE_FIRST_DYNAMIC + 120;
    RudaMirRegister leaf_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister leaf_array_value = RUDA_MIR_NO_REGISTER;
    RudaMirArrayId leaf_array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId leaf_construction =
        RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirArrayArgument argument;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirRegister returned = RUDA_MIR_NO_REGISTER;
    RudaMirRegister garbage_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister garbage = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirArrayId entry_array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId garbage_construction =
        RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    memset(leaf, 0, sizeof(*leaf));
    memset(entry, 0, sizeof(*entry));
    if (!ruda_mir_function_init(
            leaf, ruda_system_allocator(), leaf_symbol, NULL, 0, array_type,
            ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(leaf, RUDA_TYPE_F64, &leaf_value)
        || !ruda_mir_add_register(leaf, array_type, &leaf_array_value)
        || !ruda_mir_add_array(
            leaf, array_type, RUDA_TYPE_F64, &leaf_array
        ) || !ruda_mir_add_block(leaf, &block)) {
        goto failure;
    }
    argument.register_id = leaf_value;
    argument.type = RUDA_TYPE_F64;
    if (!ruda_mir_add_array_construction(
            leaf, leaf_array, &argument, 1, &leaf_construction
        )) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_F64, leaf_value, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 840
    );
    instruction.f64_bits = UINT64_C(0x4014000000000000);
    if (!ruda_mir_append_instruction(leaf, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_NEW, leaf_array_value, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 841
    );
    instruction.array = leaf_array;
    instruction.array_construction = leaf_construction;
    if (!ruda_mir_append_instruction(leaf, block, &instruction)
        || !ruda_mir_set_terminator(
            leaf, block, ruda_test_native_terminator(
                RUDA_MIR_RETURN, leaf_array_value, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 842
            )
        )) {
        goto failure;
    }
    block = RUDA_MIR_NO_BLOCK;
    if (!ruda_mir_function_init(
            entry, ruda_system_allocator(), entry_symbol, NULL, 0,
            RUDA_TYPE_F64, ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(entry, array_type, &returned)
        || !ruda_mir_add_register(entry, RUDA_TYPE_F64, &garbage_value)
        || !ruda_mir_add_register(entry, array_type, &garbage)
        || !ruda_mir_add_register(entry, RUDA_TYPE_I64, &index)
        || !ruda_mir_add_register(entry, RUDA_TYPE_F64, &result)
        || !ruda_mir_add_array(
            entry, array_type, RUDA_TYPE_F64, &entry_array
        ) || !ruda_mir_add_call(
            entry, leaf_symbol, array_type, NULL, 0, &call
        ) || !ruda_mir_add_block(entry, &block)) {
        goto failure;
    }
    argument.register_id = garbage_value;
    argument.type = RUDA_TYPE_F64;
    if (!ruda_mir_add_array_construction(
            entry, entry_array, &argument, 1, &garbage_construction
        )) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CALL, returned, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 850
    );
    instruction.call = call;
    if (!ruda_mir_append_instruction(entry, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_F64, garbage_value, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 851
    );
    instruction.f64_bits = UINT64_C(0x401c000000000000);
    if (!ruda_mir_append_instruction(entry, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_NEW, garbage, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 852
    );
    instruction.array = entry_array;
    instruction.array_construction = garbage_construction;
    if (!ruda_mir_append_instruction(entry, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, index, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 853
    );
    if (!ruda_mir_append_instruction(entry, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_GET, result, returned, index, 0, false, 854
    );
    instruction.array = entry_array;
    if (!ruda_mir_append_instruction(entry, block, &instruction)
        || !ruda_mir_set_terminator(
            entry, block, ruda_test_native_terminator(
                RUDA_MIR_RETURN, result, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 855
            )
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(entry);
    ruda_mir_function_dispose(leaf);
    return false;
}

static bool ruda_test_native_build_array_never_entry(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId callee,
    int64_t index_value
) {
    const RudaTypeId array_type = RUDA_TYPE_FIRST_DYNAMIC + 130;
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister array_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister checked = RUDA_MIR_NO_REGISTER;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId construction = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirArrayArgument argument;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirTerminator terminator;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0,
            RUDA_TYPE_NEVER, ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &value)
        || !ruda_mir_add_register(function, array_type, &array_value)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &index)
        || !ruda_mir_add_register(function, array_type, &checked)
        || !ruda_mir_add_array(function, array_type, RUDA_TYPE_F64, &array)
        || !ruda_mir_add_call(
            function, callee, RUDA_TYPE_NEVER, NULL, 0, &call
        ) || !ruda_mir_add_block(function, &block)) {
        goto failure;
    }
    argument.register_id = value;
    argument.type = RUDA_TYPE_F64;
    if (!ruda_mir_add_array_construction(
            function, array, &argument, 1, &construction
        )) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_F64, value, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 860
    );
    instruction.f64_bits = UINT64_C(0x3ff0000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_NEW, array_value, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 861
    );
    instruction.array = array;
    instruction.array_construction = construction;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, index, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, index_value, false, 862
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_REQUIRE_INDEX, checked, array_value, index,
        0, false, 863
    );
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    terminator = ruda_test_native_terminator(
        RUDA_MIR_CALL_NEVER, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_BLOCK,
        RUDA_MIR_NO_BLOCK, 866
    );
    terminator.call = call;
    if (!ruda_mir_set_terminator(function, block, terminator)) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

#if !defined(_WIN32)
/* Local child-compiler evidence only; Windows validates emitter-only tests. */
static void ruda_test_native_compile_and_run_configured(
    const RudaOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit,
    const char *configuration
) {
    static const char *const optimizations[] = {"-O0", "-O2"};
    char directory[] = "/tmp/ruda-native-c-XXXXXX";
    char source_path[512];
    char executable_path[512];
    char include_argument[1024];
    char runtime_include_argument[1024];
    char checked_source_path[1024];
    char f64_source_path[1024];
    char allocator_source_path[1024];
    char provenance_source_path[1024];
    char managed_source_path[1024];
    const char *run_arguments[2];
    RudaProcessResult process;
    FILE *file = NULL;
    int written = 0;
    size_t optimization_index = 0;
    bool source_created = false;
    if (source == NULL || source->data == NULL || mkdtemp(directory) == NULL) {
        RUDA_EXPECT(false);
        return;
    }
    written = snprintf(source_path, sizeof(source_path), "%s/program.c", directory);
    if (written < 0 || (size_t)written >= sizeof(source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        include_argument,
        sizeof(include_argument),
        "-I%s/bootstrap/include",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(include_argument)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        runtime_include_argument,
        sizeof(runtime_include_argument),
        "-I%s/runtime/include",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(runtime_include_argument)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        checked_source_path,
        sizeof(checked_source_path),
        "%s/bootstrap/src/base/checked_i64.c",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(checked_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        f64_source_path,
        sizeof(f64_source_path),
        "%s/bootstrap/src/base/f64_runtime.c",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(f64_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(allocator_source_path, sizeof(allocator_source_path),
        "%s/bootstrap/src/base/allocator.c", RUDA_TEST_SOURCE_DIR);
    if (written < 0 || (size_t)written >= sizeof(allocator_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(provenance_source_path, sizeof(provenance_source_path),
        "%s/bootstrap/src/base/provenance.c", RUDA_TEST_SOURCE_DIR);
    if (written < 0 || (size_t)written >= sizeof(provenance_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(managed_source_path, sizeof(managed_source_path),
        "%s/runtime/common/managed.c", RUDA_TEST_SOURCE_DIR);
    if (written < 0 || (size_t)written >= sizeof(managed_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    file = fopen(source_path, "wb");
    if (file == NULL) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    source_created = true;
    if (fwrite(source->data, 1, source->length, file) != source->length
        || fclose(file) != 0) {
        file = NULL;
        RUDA_EXPECT(false);
        goto cleanup;
    }
    file = NULL;
    for (optimization_index = 0;
        optimization_index < sizeof(optimizations) / sizeof(optimizations[0]);
        optimization_index += 1) {
        const char *arguments[32];
        size_t argument_count = 0;
        written = snprintf(
            executable_path,
            sizeof(executable_path),
            "%s/program-%zu",
            directory,
            optimization_index
        );
        if (written < 0 || (size_t)written >= sizeof(executable_path)) {
            RUDA_EXPECT(false);
            goto cleanup;
        }
        arguments[argument_count++] = RUDA_TEST_C_COMPILER;
        arguments[argument_count++] = "-std=c17";
        arguments[argument_count++] = optimizations[optimization_index];
#if defined(RUDA_TEST_C_SANITIZER)
        arguments[argument_count++] = RUDA_TEST_C_SANITIZER;
#endif
        arguments[argument_count++] = "-Wall";
        arguments[argument_count++] = "-Wextra";
        arguments[argument_count++] = "-Wpedantic";
        arguments[argument_count++] = "-Wconversion";
        arguments[argument_count++] = "-Wshadow";
        arguments[argument_count++] = "-Werror";
        arguments[argument_count++] = "-fno-fast-math";
        arguments[argument_count++] = "-ffp-contract=off";
        arguments[argument_count++] = "-fexcess-precision=standard";
        arguments[argument_count++] = "-DRUDA_G_COLLECT_BEFORE_ALLOCATION=1";
        if (configuration != NULL) {
            arguments[argument_count++] = configuration;
        }
        arguments[argument_count++] = include_argument;
        arguments[argument_count++] = runtime_include_argument;
        arguments[argument_count++] = source_path;
        arguments[argument_count++] = checked_source_path;
        arguments[argument_count++] = f64_source_path;
        arguments[argument_count++] = allocator_source_path;
        arguments[argument_count++] = provenance_source_path;
        arguments[argument_count++] = managed_source_path;
        arguments[argument_count++] = "-lm";
        arguments[argument_count++] = "-o";
        arguments[argument_count++] = executable_path;
        arguments[argument_count] = NULL;
        memset(&process, 0, sizeof(process));
        if (ruda_process_run(
                ruda_system_allocator(), arguments, argument_count, 16384, &process
            ) != RUDA_PROCESS_OK
            || process.exit_code != 0 || process.signaled) {
            if (process.output.data != NULL) {
                (void)fwrite(process.output.data, 1, process.output.length, stderr);
            }
            RUDA_EXPECT(false);
            ruda_process_result_dispose(&process);
            goto cleanup;
        }
        ruda_process_result_dispose(&process);

        run_arguments[0] = executable_path;
        run_arguments[1] = NULL;
        memset(&process, 0, sizeof(process));
        RUDA_EXPECT(ruda_process_run(
            ruda_system_allocator(), run_arguments, 1, 16384, &process
        ) == RUDA_PROCESS_OK);
        RUDA_EXPECT(process.exit_code == expected_exit && !process.signaled);
        RUDA_EXPECT(process.output.length == strlen(expected_output)
            && memcmp(
                process.output.data,
                expected_output,
                process.output.length
            ) == 0);
        ruda_process_result_dispose(&process);
        (void)remove(executable_path);
    }

cleanup:
    if (file != NULL) {
        (void)fclose(file);
    }
    if (source_created) {
        (void)remove(source_path);
    }
    RUDA_EXPECT(rmdir(directory) == 0);
}

static void ruda_test_native_compile_and_run(
    const RudaOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit
) {
    ruda_test_native_compile_and_run_configured(
        source, expected_output, expected_exit, NULL
    );
}
#endif

static void ruda_test_native_arrays_differential(void) {
    static const int64_t indices[] = {
        1, -1, 2, INT64_MIN, INT64_MAX
    };
#if !defined(_WIN32)
    static const char *const outputs[] = {
        "ok:f64:4022000000000000\n",
        "panic:G4002:71000000000000000000000000000000:805:806:34:"
        "617272617920696e646578202d31206973206f757473696465206c656e6774682032:"
        "1:71000000000000000000000000000000:805:806\n",
        "panic:G4002:71000000000000000000000000000000:805:806:33:"
        "617272617920696e6465782032206973206f757473696465206c656e6774682032:"
        "1:71000000000000000000000000000000:805:806\n",
        "panic:G4002:71000000000000000000000000000000:805:806:52:"
        "617272617920696e646578202d39323233333732303336383534373735383038206973206f757473696465206c656e6774682032:"
        "1:71000000000000000000000000000000:805:806\n",
        "panic:G4002:71000000000000000000000000000000:805:806:51:"
        "617272617920696e6465782039323233333732303336383534373735383037206973206f757473696465206c656e6774682032:"
        "1:71000000000000000000000000000000:805:806\n"
    };
#endif
    const RudaSymbolId symbol = {{0x71}};
    size_t case_index = 0;
    for (case_index = 0;
        case_index < sizeof(indices) / sizeof(indices[0]);
        case_index += 1) {
        RudaMirFunction function;
        RudaMirExecution mir_execution;
        RudaMirInterpretOptions mir_options;
        RudaMirPanic mir_panic;
        RudaMirValue mir_result;
        RudaBytecodeFunction lowered;
        RudaBytecodeFunction decoded;
        RudaBytecodeExecution vm_execution;
        RudaBytecodeRunOptions vm_options;
        RudaBytecodePanic vm_panic;
        RudaBytecodeValue vm_result;
        RudaOwnedBytes encoded;
        RudaOwnedBytes source;
        RudaNativeCProgram program;
        const RudaMirFunction *functions[1];
        memset(&function, 0, sizeof(function));
        memset(&mir_execution, 0, sizeof(mir_execution));
        memset(&mir_panic, 0, sizeof(mir_panic));
        memset(&lowered, 0, sizeof(lowered));
        memset(&decoded, 0, sizeof(decoded));
        memset(&vm_execution, 0, sizeof(vm_execution));
        memset(&vm_panic, 0, sizeof(vm_panic));
        memset(&encoded, 0, sizeof(encoded));
        memset(&source, 0, sizeof(source));
        RUDA_EXPECT(ruda_test_native_build_array(
            &function, symbol, indices[case_index]
        ));
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
        RUDA_EXPECT(ruda_mir_execution_init(
            &mir_execution, ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        mir_options = ruda_mir_interpret_default_options();
        mir_options.execution = &mir_execution;
        mir_options.panic = &mir_panic;
        memset(&mir_result, 0, sizeof(mir_result));
        if (indices[case_index] == 1) {
            RUDA_EXPECT(ruda_mir_interpret(
                &function, NULL, 0, ruda_system_allocator(), mir_options,
                &mir_result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_F64
                && mir_result.as.f64_bits == UINT64_C(0x4022000000000000));
        } else {
            RUDA_EXPECT(ruda_mir_interpret(
                &function, NULL, 0, ruda_system_allocator(), mir_options,
                &mir_result
            ) == RUDA_MIR_INTERPRET_PANIC);
            RUDA_EXPECT(mir_panic.code != NULL
                && strcmp(mir_panic.code, "G4002") == 0
                && mir_panic.span.start == 805 && mir_panic.span.end == 806
                && memcmp(&mir_panic.symbol, &symbol, sizeof(symbol)) == 0);
        }
        RUDA_EXPECT(ruda_mir_lower_bytecode_function(
            &function, ruda_system_allocator(),
            ruda_mir_bytecode_lower_default_options(), &lowered
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        RUDA_EXPECT(ruda_bytecode_encode_function(
            &lowered, ruda_system_allocator(), &encoded
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_decode_function(
            encoded.data, encoded.length, ruda_system_allocator(),
            ruda_bytecode_function_default_options(), &decoded
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_execution_init(
            &vm_execution, ruda_system_allocator(),
            ruda_bytecode_execution_default_options()
        ));
        RUDA_EXPECT(ruda_bytecode_panic_init(
            &vm_panic, ruda_system_allocator()
        ));
        vm_options = ruda_bytecode_run_default_options();
        vm_options.execution = &vm_execution;
        vm_options.panic = &vm_panic;
        memset(&vm_result, 0, sizeof(vm_result));
        if (indices[case_index] == 1) {
            RUDA_EXPECT(ruda_bytecode_run(
                &decoded, NULL, 0, ruda_system_allocator(), vm_options,
                &vm_result
            ) == RUDA_BYTECODE_RUN_OK);
            RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_F64
                && vm_result.as.f64_bits == UINT64_C(0x4022000000000000));
        } else {
            RUDA_EXPECT(ruda_bytecode_run(
                &decoded, NULL, 0, ruda_system_allocator(), vm_options,
                &vm_result
            ) == RUDA_BYTECODE_RUN_PANIC);
            RUDA_EXPECT(vm_panic.code != NULL
                && strcmp(vm_panic.code, "G4002") == 0
                && vm_panic.span.start == 805 && vm_panic.span.end == 806
                && vm_panic.frames.length == 1);
            RUDA_EXPECT(vm_panic.message.length == mir_panic.message.length);
            if (vm_panic.message.length == mir_panic.message.length) {
                RUDA_EXPECT(memcmp(
                    vm_panic.message.data,
                    mir_panic.message.data,
                    mir_panic.message.length
                ) == 0);
            }
        }
        functions[0] = &function;
        memset(&program, 0, sizeof(program));
        program.functions = functions;
        program.function_count = 1;
        program.entry_symbol = symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &source
        ) == RUDA_NATIVE_C_EMIT_OK);
        RUDA_EXPECT(ruda_test_native_contains(
            &source, "ruda_managed_array_new"
        ));
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(
            &source, outputs[case_index], indices[case_index] == 1 ? 0 : 1
        );
        if (indices[case_index] == 1) {
            ruda_test_native_compile_and_run_configured(
                &source, "trap:5:0:0\n", 1,
                "-DRUDA_G_FAIL_ALLOCATION_AT=0"
            );
            ruda_test_native_compile_and_run_configured(
                &source, "trap:5:0:0\n", 1,
                "-DRUDA_G_FAIL_ALLOCATION_AT=1"
            );
        }
#endif
        ruda_owned_bytes_dispose(&source);
        ruda_bytecode_panic_dispose(&vm_panic);
        ruda_bytecode_execution_dispose(&vm_execution);
        ruda_bytecode_function_dispose(&decoded);
        ruda_owned_bytes_dispose(&encoded);
        ruda_bytecode_function_dispose(&lowered);
        ruda_mir_panic_dispose(&mir_panic);
        ruda_mir_execution_dispose(&mir_execution);
        ruda_mir_function_dispose(&function);
    }
    {
        const RudaSymbolId nested_symbol = {{0x72}};
        RudaMirFunction function;
        RudaMirExecution mir_execution;
        RudaMirInterpretOptions mir_options;
        RudaMirValue mir_result;
        RudaBytecodeFunction lowered;
        RudaBytecodeExecution vm_execution;
        RudaBytecodeRunOptions vm_options;
        RudaBytecodeValue vm_result;
        RudaOwnedBytes source;
        RudaNativeCProgram program;
        const RudaMirFunction *functions[1];
        memset(&function, 0, sizeof(function));
        memset(&mir_execution, 0, sizeof(mir_execution));
        memset(&lowered, 0, sizeof(lowered));
        memset(&vm_execution, 0, sizeof(vm_execution));
        memset(&source, 0, sizeof(source));
        RUDA_EXPECT(ruda_test_native_build_nested_array(
            &function, nested_symbol
        ));
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
        RUDA_EXPECT(ruda_mir_execution_init(
            &mir_execution, ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        mir_options = ruda_mir_interpret_default_options();
        mir_options.execution = &mir_execution;
        memset(&mir_result, 0, sizeof(mir_result));
        RUDA_EXPECT(ruda_mir_interpret(
            &function, NULL, 0, ruda_system_allocator(), mir_options,
            &mir_result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(mir_result.type == RUDA_TYPE_F64
            && mir_result.as.f64_bits == UINT64_C(0x400c000000000000));
        RUDA_EXPECT(ruda_mir_lower_bytecode_function(
            &function, ruda_system_allocator(),
            ruda_mir_bytecode_lower_default_options(), &lowered
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        {
            RudaBytecodeExecutionOptions execution_options =
                ruda_bytecode_execution_default_options();
            execution_options.managed_heap.collect_before_allocation = true;
            RUDA_EXPECT(ruda_bytecode_execution_init(
                &vm_execution, ruda_system_allocator(), execution_options
            ));
        }
        vm_options = ruda_bytecode_run_default_options();
        vm_options.execution = &vm_execution;
        memset(&vm_result, 0, sizeof(vm_result));
        RUDA_EXPECT(ruda_bytecode_run(
            &lowered, NULL, 0, ruda_system_allocator(), vm_options,
            &vm_result
        ) == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_F64
            && vm_result.as.f64_bits == mir_result.as.f64_bits);
        functions[0] = &function;
        memset(&program, 0, sizeof(program));
        program.functions = functions;
        program.function_count = 1;
        program.entry_symbol = nested_symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &source
        ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(
            &source, "ok:f64:400c000000000000\n", 0
        );
        ruda_test_native_compile_and_run_configured(
            &source, "trap:6:0:0\n", 1,
            "-DRUDA_G_MAX_OBJECTS=1"
        );
#endif
        ruda_owned_bytes_dispose(&source);
        ruda_bytecode_execution_dispose(&vm_execution);
        ruda_bytecode_function_dispose(&lowered);
        ruda_mir_execution_dispose(&mir_execution);
        ruda_mir_function_dispose(&function);
    }
    {
        const RudaSymbolId leaf_symbol = {{0x73}};
        const RudaSymbolId entry_symbol = {{0x74}};
        RudaMirFunction leaf;
        RudaMirFunction entry;
        const RudaMirFunction *functions[2];
        RudaBytecodeProgram bytecode;
        RudaBytecodeExecution execution;
        RudaBytecodeExecutionOptions execution_options;
        RudaBytecodeRunOptions run_options;
        RudaBytecodeValue result;
        RudaNativeCProgram native_program;
        RudaOwnedBytes source;
        memset(&leaf, 0, sizeof(leaf));
        memset(&entry, 0, sizeof(entry));
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&execution, 0, sizeof(execution));
        memset(&source, 0, sizeof(source));
        RUDA_EXPECT(ruda_test_native_build_array_call(
            &leaf, &entry, leaf_symbol, entry_symbol
        ));
        RUDA_EXPECT(ruda_mir_verify(&leaf) == RUDA_MIR_VERIFY_OK);
        RUDA_EXPECT(ruda_mir_verify(&entry) == RUDA_MIR_VERIFY_OK);
        functions[0] = &leaf;
        functions[1] = &entry;
        RUDA_EXPECT(ruda_mir_lower_bytecode_program(
            functions, 2, entry_symbol, ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(), &bytecode
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        execution_options = ruda_bytecode_execution_default_options();
        execution_options.managed_heap.collect_before_allocation = true;
        RUDA_EXPECT(ruda_bytecode_execution_init(
            &execution, ruda_system_allocator(), execution_options
        ));
        run_options = ruda_bytecode_run_default_options();
        run_options.execution = &execution;
        memset(&result, 0, sizeof(result));
        RUDA_EXPECT(ruda_bytecode_run_program(
            &bytecode, NULL, 0, ruda_system_allocator(), run_options, &result
        ) == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_F64
            && result.as.f64_bits == UINT64_C(0x4014000000000000));
        memset(&native_program, 0, sizeof(native_program));
        native_program.functions = functions;
        native_program.function_count = 2;
        native_program.entry_symbol = entry_symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &native_program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &source
        ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(
            &source, "ok:f64:4014000000000000\n", 0
        );
#endif
        ruda_owned_bytes_dispose(&source);
        ruda_bytecode_execution_dispose(&execution);
        ruda_bytecode_program_dispose(&bytecode);
        ruda_mir_function_dispose(&entry);
        ruda_mir_function_dispose(&leaf);
    }
}

static void ruda_test_native_emit(
    const RudaMirFunction *const *functions,
    size_t count,
    RudaSymbolId entry,
    RudaNativeCEmitOptions options,
    RudaNativeCEmitStatus expected
) {
    RudaNativeCProgram program;
    RudaOwnedBytes output;
    memset(&program, 0, sizeof(program));
    memset(&output, 0, sizeof(output));
    program.functions = functions;
    program.function_count = count;
    program.entry_symbol = entry;
    RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
        options, &output) == expected);
    if (expected != RUDA_NATIVE_C_EMIT_OK) {
        RUDA_EXPECT(output.data == NULL && output.length == 0);
    }
    ruda_owned_bytes_dispose(&output);
}

static void ruda_test_native_vm_program(
    const RudaMirFunction *const *functions,
    RudaSymbolId entry_symbol,
    int64_t expected
) {
    RudaBytecodeProgram program;
    RudaBytecodeValue result;
    memset(&program, 0, sizeof(program));
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = INT64_MIN;
    RUDA_EXPECT(ruda_mir_lower_bytecode_program(functions, 2, entry_symbol,
        ruda_system_allocator(), ruda_mir_bytecode_program_lower_default_options(),
        &program) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == expected);
    ruda_bytecode_program_dispose(&program);
}

static void ruda_test_native_vm_overflow(const RudaMirFunction *input) {
    RudaBytecodeFunction function;
    RudaBytecodeValue result;
    RudaBytecodeTrap trap;
    RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
    memset(&function, 0, sizeof(function));
    memset(&trap, 0, sizeof(trap));
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = 1234;
    options.trap = &trap;
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(input, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &function)
        == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OVERFLOW);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    RUDA_EXPECT(trap.has_span && trap.span.start == 32 && trap.span.end == 33);
    ruda_bytecode_function_dispose(&function);
}

typedef enum RudaTestNativeScalarOutcome {
    RUDA_TEST_NATIVE_SCALAR_I64,
    RUDA_TEST_NATIVE_SCALAR_BOOL,
    RUDA_TEST_NATIVE_SCALAR_OVERFLOW,
    RUDA_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO,
    RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT
} RudaTestNativeScalarOutcome;

typedef struct RudaTestNativeScalarCase {
    RudaMirInstructionKind kind;
    int64_t left;
    int64_t right;
    bool left_boolean;
    bool right_boolean;
    RudaTestNativeScalarOutcome outcome;
    int64_t expected_i64;
    bool expected_boolean;
    bool native;
} RudaTestNativeScalarCase;

static bool ruda_test_native_scalar_is_boolean(RudaMirInstructionKind kind) {
    return kind == RUDA_MIR_NOT_BOOL || kind == RUDA_MIR_EQUAL_BOOL
        || kind == RUDA_MIR_EQUAL_I64 || kind == RUDA_MIR_LESS_I64
        || kind == RUDA_MIR_LESS_EQUAL_I64;
}

static bool ruda_test_native_scalar_is_boolean_input(
    RudaMirInstructionKind kind
) {
    return kind == RUDA_MIR_NOT_BOOL || kind == RUDA_MIR_EQUAL_BOOL;
}

static bool ruda_test_native_scalar_is_unary(RudaMirInstructionKind kind) {
    return kind == RUDA_MIR_NOT_BOOL || kind == RUDA_MIR_NEGATE_CHECKED_I64
        || kind == RUDA_MIR_BITWISE_NOT_I64;
}

static bool ruda_test_native_build_scalar(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    const RudaTestNativeScalarCase *test_case
) {
    RudaTypeId input_type = ruda_test_native_scalar_is_boolean_input(
        test_case->kind
    ) ? RUDA_TYPE_BOOL : RUDA_TYPE_I64;
    RudaTypeId result_type = ruda_test_native_scalar_is_boolean(
        test_case->kind
    ) ? RUDA_TYPE_BOOL : RUDA_TYPE_I64;
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, result_type, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, input_type, &left)
        || (!ruda_test_native_scalar_is_unary(test_case->kind)
            && !ruda_mir_add_register(function, input_type, &right))
        || !ruda_mir_add_register(function, result_type, &result)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    if (ruda_test_native_scalar_is_unary(test_case->kind)) {
        right = left;
    }
    instruction = ruda_test_native_instruction(
        input_type == RUDA_TYPE_BOOL ? RUDA_MIR_CONSTANT_BOOL
            : RUDA_MIR_CONSTANT_I64,
        left, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, test_case->left,
        test_case->left_boolean, 300
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    if (!ruda_test_native_scalar_is_unary(test_case->kind)) {
        instruction = ruda_test_native_instruction(
            input_type == RUDA_TYPE_BOOL ? RUDA_MIR_CONSTANT_BOOL
                : RUDA_MIR_CONSTANT_I64,
            right, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, test_case->right,
            test_case->right_boolean, 301
        );
        if (!ruda_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    }
    instruction = ruda_test_native_instruction(test_case->kind, result, left,
        right, 0, false, 302);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 303))) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static void ruda_test_native_scalar_differential(void) {
    static const RudaTestNativeScalarCase cases[] = {
        {RUDA_MIR_NOT_BOOL, 0, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, true},
        {RUDA_MIR_EQUAL_BOOL, 0, 0, true, true,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {RUDA_MIR_EQUAL_I64, -1, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {RUDA_MIR_LESS_I64, INT64_MIN, INT64_MAX, false, false,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {RUDA_MIR_LESS_EQUAL_I64, 7, 7, false, false,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {RUDA_MIR_BITWISE_NOT_I64, -1, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 0, false, true},
        {RUDA_MIR_BITWISE_AND_I64, -1, 0x55, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 0x55, false, false},
        {RUDA_MIR_BITWISE_XOR_I64, -1, 0x55, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, ~INT64_C(0x55), false, false},
        {RUDA_MIR_BITWISE_OR_I64, INT64_MIN, 7, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, INT64_MIN | 7, false, false},
        {RUDA_MIR_ADD_CHECKED_I64, 2, 3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 5, false, false},
        {RUDA_MIR_ADD_CHECKED_I64, INT64_MAX, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, true},
        {RUDA_MIR_SUBTRACT_CHECKED_I64, INT64_MIN, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_MULTIPLY_CHECKED_I64, -7, 3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -21, false, false},
        {RUDA_MIR_MULTIPLY_CHECKED_I64, INT64_MIN, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, true},
        {RUDA_MIR_NEGATE_CHECKED_I64, INT64_MIN, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_DIVIDE_CHECKED_I64, -7, 3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -2, false, false},
        {RUDA_MIR_DIVIDE_CHECKED_I64, 7, -3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -2, false, false},
        {RUDA_MIR_DIVIDE_CHECKED_I64, 1, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO, 0, false, true},
        {RUDA_MIR_DIVIDE_CHECKED_I64, INT64_MIN, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_REMAINDER_CHECKED_I64, -7, 3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -1, false, false},
        {RUDA_MIR_REMAINDER_CHECKED_I64, 7, -3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 1, false, false},
        {RUDA_MIR_REMAINDER_CHECKED_I64, 1, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO, 0, false, false},
        {RUDA_MIR_REMAINDER_CHECKED_I64, INT64_MIN, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 1, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 2, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 62, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, INT64_C(1) << 62, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 63, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 64, false, false,
            RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, true},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MAX, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -8, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -4, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 62, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -1, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 63, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -1, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 64, false, false,
            RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, true}
    };
    const RudaSymbolId symbol = {{0x30}};
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaMirFunction input;
        RudaMirValue mir_result;
        RudaMirInterpretStatus mir_status;
        RudaBytecodeFunction bytecode;
        RudaBytecodeValue bytecode_result;
        RudaBytecodeTrap trap;
        RudaBytecodeRunStatus bytecode_status;
        RudaOwnedBytes source;
        const RudaMirFunction *functions[1];
        RudaNativeCProgram program;
        char expected_output[64];
#if !defined(_WIN32)
        uint32_t expected_exit = 0;
#endif
        int native_status = 0;
        memset(&input, 0, sizeof(input));
        memset(&mir_result, 0, sizeof(mir_result));
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&bytecode_result, 0, sizeof(bytecode_result));
        memset(&trap, 0, sizeof(trap));
        memset(&source, 0, sizeof(source));
        RUDA_EXPECT(ruda_test_native_build_scalar(&input, symbol, &cases[index]));
        mir_status = ruda_mir_interpret(&input, NULL, 0,
            ruda_system_allocator(), ruda_mir_interpret_default_options(),
            &mir_result);
        RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input,
            ruda_system_allocator(), ruda_mir_bytecode_lower_default_options(),
            &bytecode) == RUDA_MIR_BYTECODE_LOWER_OK);
        {
            RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
            options.trap = &trap;
            bytecode_result.type = RUDA_BYTECODE_TYPE_I64;
            bytecode_result.as.i64 = 1234;
            bytecode_status = ruda_bytecode_run(&bytecode, NULL, 0,
                ruda_system_allocator(), options, &bytecode_result);
        }
        if (cases[index].outcome == RUDA_TEST_NATIVE_SCALAR_I64) {
            RUDA_EXPECT(mir_status == RUDA_MIR_INTERPRET_OK
                && bytecode_status == RUDA_BYTECODE_RUN_OK);
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_I64
                && mir_result.as.i64 == cases[index].expected_i64);
            RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_I64
                && bytecode_result.as.i64 == cases[index].expected_i64);
            (void)snprintf(expected_output, sizeof(expected_output), "ok:i64:%lld\n",
                (long long)cases[index].expected_i64);
        } else if (cases[index].outcome == RUDA_TEST_NATIVE_SCALAR_BOOL) {
            RUDA_EXPECT(mir_status == RUDA_MIR_INTERPRET_OK
                && bytecode_status == RUDA_BYTECODE_RUN_OK);
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_BOOL
                && mir_result.as.boolean == cases[index].expected_boolean);
            RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_BOOL
                && bytecode_result.as.boolean == cases[index].expected_boolean);
            (void)snprintf(expected_output, sizeof(expected_output), "ok:bool:%s\n",
                cases[index].expected_boolean ? "true" : "false");
        } else {
            RudaMirInterpretStatus expected_mir = RUDA_MIR_INTERPRET_OVERFLOW;
            RudaBytecodeRunStatus expected_bytecode = RUDA_BYTECODE_RUN_OVERFLOW;
            if (cases[index].outcome
                == RUDA_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO) {
                expected_mir = RUDA_MIR_INTERPRET_DIVISION_BY_ZERO;
                expected_bytecode = RUDA_BYTECODE_RUN_DIVISION_BY_ZERO;
                native_status = 2;
            } else if (cases[index].outcome
                == RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT) {
                expected_mir = RUDA_MIR_INTERPRET_INVALID_SHIFT;
                expected_bytecode = RUDA_BYTECODE_RUN_INVALID_SHIFT;
                native_status = 3;
            } else {
                native_status = 1;
            }
            RUDA_EXPECT(mir_status == expected_mir
                && bytecode_status == expected_bytecode);
            RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_I64
                && bytecode_result.as.i64 == 1234 && trap.has_span
                && trap.span.start == 302 && trap.span.end == 303);
            (void)snprintf(expected_output, sizeof(expected_output),
                "trap:%d:302:303\n", native_status);
#if !defined(_WIN32)
            expected_exit = 1;
#endif
        }
        if (cases[index].native) {
            functions[0] = &input;
            program.functions = functions;
            program.function_count = 1;
            program.entry_symbol = symbol;
            RUDA_EXPECT(ruda_native_c_emit_program(&program,
                ruda_system_allocator(), ruda_native_c_emit_default_options(),
                &source) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
            ruda_test_native_compile_and_run(&source, expected_output, expected_exit);
#endif
            ruda_owned_bytes_dispose(&source);
        }
        ruda_bytecode_function_dispose(&bytecode);
        ruda_mir_function_dispose(&input);
    }
}

static bool ruda_test_native_f64_is_boolean(RudaMirInstructionKind kind) {
    return kind == RUDA_MIR_EQUAL_F64 || kind == RUDA_MIR_LESS_F64
        || kind == RUDA_MIR_LESS_EQUAL_F64;
}

static bool ruda_test_native_f64_is_unary(RudaMirInstructionKind kind) {
    return kind == RUDA_MIR_NEGATE_F64;
}

static bool ruda_test_native_build_f64_operation(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaMirInstructionKind kind,
    uint64_t left_bits,
    uint64_t right_bits
) {
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaTypeId result_type = ruda_test_native_f64_is_boolean(kind)
        ? RUDA_TYPE_BOOL : RUDA_TYPE_F64;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, result_type, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &left)
        || (!ruda_test_native_f64_is_unary(kind)
            && !ruda_mir_add_register(function, RUDA_TYPE_F64, &right))
        || !ruda_mir_add_register(function, result_type, &result)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    if (ruda_test_native_f64_is_unary(kind)) {
        right = left;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_F64, left,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 500);
    instruction.f64_bits = left_bits;
    if (!ruda_mir_append_instruction(function, block, &instruction)) goto failure;
    if (!ruda_test_native_f64_is_unary(kind)) {
        instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_F64, right,
            RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 501);
        instruction.f64_bits = right_bits;
        if (!ruda_mir_append_instruction(function, block, &instruction)) goto failure;
    }
    instruction = ruda_test_native_instruction(kind, result, left, right,
        0, false, 502);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 503))) goto failure;
    return true;
failure:
    ruda_mir_function_dispose(function);
    return false;
}

static bool ruda_test_native_build_f64_identity(
    RudaMirFunction *function, RudaSymbolId symbol
) {
    const RudaTypeId parameter = RUDA_TYPE_F64;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol,
            &parameter, 1, RUDA_TYPE_F64, ruda_mir_function_default_options())
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, 0,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 510))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_native_build_f64_direct_entry(
    RudaMirFunction *function, RudaSymbolId symbol, RudaSymbolId callee
) {
    const RudaMirCallArgument argument = {0, RUDA_TYPE_F64};
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_F64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &value)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &result)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_call(function, callee, RUDA_TYPE_F64, &argument, 1, &call)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_F64, value,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 520);
    instruction.f64_bits = UINT64_C(0x8000000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) goto failure;
    instruction = ruda_test_native_instruction(RUDA_MIR_CALL, result,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 521);
    instruction.call = call;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 522))) goto failure;
    return true;
failure:
    ruda_mir_function_dispose(function);
    return false;
}

static void ruda_test_native_f64_differential(void) {
    typedef struct RudaTestNativeF64Case {
        RudaMirInstructionKind kind;
        uint64_t left;
        uint64_t right;
        bool boolean;
        uint64_t expected_bits;
    } RudaTestNativeF64Case;
    static const RudaTestNativeF64Case cases[] = {
        {RUDA_MIR_ADD_F64, UINT64_C(0x3ff8000000000000), UINT64_C(0x4002000000000000), false, UINT64_C(0x400e000000000000)},
        {RUDA_MIR_DIVIDE_F64, UINT64_C(0x3ff0000000000000), UINT64_C(0x8000000000000000), false, UINT64_C(0xfff0000000000000)},
        {RUDA_MIR_MULTIPLY_F64, UINT64_C(0x7ff0000000000000), UINT64_C(0x0000000000000000), false, UINT64_C(0x7ff8000000000000)},
        {RUDA_MIR_NEGATE_F64, UINT64_C(0x7ff0000000000001), 0, false, UINT64_C(0xfff0000000000001)},
        {RUDA_MIR_EQUAL_F64, UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000), true, 1},
        {RUDA_MIR_EQUAL_F64, UINT64_C(0x7ff8000000000000), UINT64_C(0x7ff8000000000000), true, 0},
        {RUDA_MIR_LESS_F64, UINT64_C(0x7ff8000000000000), UINT64_C(0x3ff0000000000000), true, 0},
        {RUDA_MIR_LESS_EQUAL_F64, UINT64_C(0xbff0000000000000), UINT64_C(0xbff0000000000000), true, 1}
    };
    const RudaSymbolId symbol = {{0x50}};
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaMirFunction function;
        RudaMirValue mir_result;
        RudaBytecodeFunction bytecode;
        RudaBytecodeValue vm_result;
        RudaOwnedBytes source;
        const RudaMirFunction *functions[1];
        RudaNativeCProgram program;
        char expected[64];
        memset(&function, 0, sizeof(function)); memset(&mir_result, 0, sizeof(mir_result));
        memset(&bytecode, 0, sizeof(bytecode)); memset(&vm_result, 0, sizeof(vm_result));
        memset(&source, 0, sizeof(source));
        RUDA_EXPECT(ruda_test_native_build_f64_operation(&function, symbol,
            cases[index].kind, cases[index].left, cases[index].right));
        RUDA_EXPECT(ruda_mir_interpret(&function, NULL, 0, ruda_system_allocator(),
            ruda_mir_interpret_default_options(), &mir_result) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_lower_bytecode_function(&function, ruda_system_allocator(),
            ruda_mir_bytecode_lower_default_options(), &bytecode) == RUDA_MIR_BYTECODE_LOWER_OK);
        RUDA_EXPECT(ruda_bytecode_run(&bytecode, NULL, 0, ruda_system_allocator(),
            ruda_bytecode_run_default_options(), &vm_result) == RUDA_BYTECODE_RUN_OK);
        if (cases[index].boolean) {
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_BOOL && mir_result.as.boolean == (cases[index].expected_bits != 0));
            RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_BOOL && vm_result.as.boolean == (cases[index].expected_bits != 0));
            (void)snprintf(expected, sizeof(expected), "ok:bool:%s\n", cases[index].expected_bits != 0 ? "true" : "false");
        } else {
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_F64 && mir_result.as.f64_bits == cases[index].expected_bits);
            RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_F64 && vm_result.as.f64_bits == cases[index].expected_bits);
            (void)snprintf(expected, sizeof(expected), "ok:f64:%016llx\n", (unsigned long long)cases[index].expected_bits);
        }
        functions[0] = &function;
        program.functions = functions; program.function_count = 1; program.entry_symbol = symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &source) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&source, expected, 0);
#endif
        ruda_owned_bytes_dispose(&source); ruda_bytecode_function_dispose(&bytecode);
        ruda_mir_function_dispose(&function);
    }
}

static void ruda_test_native_f64_direct_call(void) {
    const RudaSymbolId callee_symbol = {{0x51}};
    const RudaSymbolId entry_symbol = {{0x52}};
    RudaMirFunction callee;
    RudaMirFunction entry;
    const RudaMirFunction *functions[2];
    RudaBytecodeProgram bytecode;
    RudaBytecodeValue result;
    RudaOwnedBytes first;
    RudaOwnedBytes second;
    RudaNativeCProgram program;
    memset(&callee, 0, sizeof(callee)); memset(&entry, 0, sizeof(entry));
    memset(&bytecode, 0, sizeof(bytecode)); memset(&result, 0, sizeof(result));
    memset(&first, 0, sizeof(first)); memset(&second, 0, sizeof(second));
    RUDA_EXPECT(ruda_test_native_build_f64_identity(&callee, callee_symbol));
    RUDA_EXPECT(ruda_test_native_build_f64_direct_entry(&entry, entry_symbol,
        callee_symbol));
    functions[0] = &callee; functions[1] = &entry;
    RUDA_EXPECT(ruda_mir_lower_bytecode_program(functions, 2, entry_symbol,
        ruda_system_allocator(), ruda_mir_bytecode_program_lower_default_options(),
        &bytecode) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_run_program(&bytecode, NULL, 0,
        ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x8000000000000000));
    ruda_bytecode_program_dispose(&bytecode);
    program.functions = functions; program.function_count = 2;
    program.entry_symbol = entry_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &first) == RUDA_NATIVE_C_EMIT_OK);
    {
        const RudaMirFunction *permuted[2] = {&entry, &callee};
        program.functions = permuted;
        RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &second) == RUDA_NATIVE_C_EMIT_OK);
    }
    RUDA_EXPECT(first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first, "ok:f64:8000000000000000\n", 0);
#endif
    ruda_owned_bytes_dispose(&second); ruda_owned_bytes_dispose(&first);
    ruda_mir_function_dispose(&entry); ruda_mir_function_dispose(&callee);
}

static void ruda_test_native_f64_source_differential(void) {
    static const char source[] =
        "fn main(): f64 {\n"
        "  let value: f64 = 3.0;\n"
        "  value += 1.0;\n"
        "  value *= 2.0;\n"
        "  return value / 4.0;\n"
        "}\n";
    static const uint64_t expected_bits = UINT64_C(0x4000000000000000);
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot = NULL;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaAstTree ast;
    RudaCheckResult checked;
    const RudaCheckedFunction *entry = NULL;
    const RudaMirFunction *functions[1];
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeValue vm_result;
    RudaNativeCProgram native_program;
    RudaOwnedBytes generated;
    bool sources_initialized = false;
    bool lexed_initialized = false;
    bool parsed_initialized = false;
    bool ast_initialized = false;
    bool checked_initialized = false;
    bool program_initialized = false;
    bool decoded_initialized = false;
    memset(&sources, 0, sizeof(sources));
    memset(&lexed, 0, sizeof(lexed));
    memset(&parsed, 0, sizeof(parsed));
    memset(&ast, 0, sizeof(ast));
    memset(&checked, 0, sizeof(checked));
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&generated, 0, sizeof(generated));
    if (!ruda_source_manager_init(&sources, ruda_system_allocator())) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    sources_initialized = true;
    if (ruda_source_manager_add(
            &sources,
            "source-f64.ruda",
            sizeof("source-f64.ruda") - 1,
            source,
            sizeof(source) - 1,
            &snapshot
        ) != RUDA_SOURCE_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    if (ruda_lex_snapshot(snapshot, ruda_system_allocator(),
            ruda_lex_default_options(), &lexed) != RUDA_LEX_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    lexed_initialized = true;
    RUDA_EXPECT(lexed.diagnostics.length == 0);
    if (ruda_parse_file(&lexed, ruda_system_allocator(),
            ruda_parse_default_options(), &parsed) != RUDA_PARSE_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    parsed_initialized = true;
    RUDA_EXPECT(parsed.diagnostics.length == 0);
    if (ruda_ast_project(&parsed.tree, ruda_system_allocator(),
            ruda_ast_default_options(), &ast) != RUDA_AST_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    ast_initialized = true;
    if (ruda_check_ast(&ast, "test/source-f64", ruda_system_allocator(),
            ruda_check_default_options(), &checked) != RUDA_CHECK_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    checked_initialized = true;
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.functions.length == 1);
    entry = ruda_checked_function(&checked, 0);
    RUDA_EXPECT(entry != NULL);
    if (entry == NULL) {
        goto cleanup;
    }
    functions[0] = &entry->mir;
    if (ruda_mir_lower_bytecode_program(
            functions,
            1,
            entry->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(),
            &program
        ) != RUDA_MIR_BYTECODE_LOWER_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    program_initialized = true;
    if (ruda_bytecode_encode_program(
            &program,
            ruda_system_allocator(),
            &encoded
        ) != RUDA_BYTECODE_CODEC_OK
        || ruda_bytecode_decode_program(
            encoded.data,
            encoded.length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &decoded
        ) != RUDA_BYTECODE_CODEC_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    decoded_initialized = true;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &decoded,
        NULL,
        0,
        ruda_system_allocator(),
        ruda_bytecode_run_default_options(),
        &vm_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_F64
        && vm_result.as.f64_bits == expected_bits);
    native_program.functions = functions;
    native_program.function_count = 1;
    native_program.entry_symbol = entry->mir.symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program,
        ruda_system_allocator(),
        ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        ruda_test_native_compile_and_run(
            &generated,
            "ok:f64:4000000000000000\n",
            0
        );
    }
#endif

cleanup:
    ruda_owned_bytes_dispose(&generated);
    if (decoded_initialized) {
        ruda_bytecode_program_dispose(&decoded);
    }
    ruda_owned_bytes_dispose(&encoded);
    if (program_initialized) {
        ruda_bytecode_program_dispose(&program);
    }
    if (checked_initialized) {
        ruda_check_result_dispose(&checked);
    }
    if (ast_initialized) {
        ruda_ast_tree_dispose(&ast);
    }
    if (parsed_initialized) {
        ruda_parse_result_dispose(&parsed);
    }
    if (lexed_initialized) {
        ruda_lex_result_dispose(&lexed);
    }
    if (sources_initialized) {
        ruda_source_manager_dispose(&sources);
    }
}

static bool ruda_test_native_build_string_return(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    static const unsigned char value[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirStringId constant = RUDA_MIR_NO_STRING;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_STRING, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_STRING, &result)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_string_constant(function, value, sizeof(value),
            &constant)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_STRING, result,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 400);
    instruction.string_constant = constant;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 401))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_native_build_string_equal(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    static const unsigned char value[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirStringId first = RUDA_MIR_NO_STRING;
    RudaMirStringId second = RUDA_MIR_NO_STRING;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_BOOL, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_STRING, &left)
        || !ruda_mir_add_register(function, RUDA_TYPE_STRING, &right)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &result)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_string_constant(function, value, sizeof(value), &first)
        || !ruda_mir_add_string_constant(function, value, sizeof(value), &second)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_STRING, left,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 410);
    instruction.string_constant = first;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_STRING, right,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 411);
    instruction.string_constant = second;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_EQUAL_STRING, result,
        left, right, 0, false, 412);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 413))) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static bool ruda_test_native_build_panic_leaf(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirStringId constant = RUDA_MIR_NO_STRING;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_NEVER, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_STRING, &value)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_string_constant(function, message, sizeof(message),
            &constant)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_STRING, value,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 200);
    instruction.string_constant = constant;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_PANIC, value,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 201))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_native_array_rhs_ordering(void) {
    const RudaSymbolId leaf_symbol = {{0x75}};
    const RudaSymbolId entry_symbol = {{0x76}};
    static const int64_t indices[] = {-1, 0};
#if !defined(_WIN32)
    static const char *const outputs[] = {
        "panic:G4002:76000000000000000000000000000000:863:864:34:"
        "617272617920696e646578202d31206973206f757473696465206c656e6774682031:"
        "1:76000000000000000000000000000000:863:864\n",
        "panic:G4000:75000000000000000000000000000000:201:202:6:700021e29883:2:"
        "75000000000000000000000000000000:201:202:"
        "76000000000000000000000000000000:866:867\n"
    };
#endif
    size_t index = 0;
    for (index = 0; index < sizeof(indices) / sizeof(indices[0]); index += 1) {
        RudaMirFunction leaf;
        RudaMirFunction entry;
        const RudaMirFunction *functions[2];
        RudaBytecodeProgram bytecode;
        RudaBytecodeExecution execution;
        RudaBytecodePanic panic;
        RudaBytecodeRunOptions run_options;
        RudaBytecodeValue result;
        RudaNativeCProgram native_program;
        RudaOwnedBytes source;
        memset(&leaf, 0, sizeof(leaf));
        memset(&entry, 0, sizeof(entry));
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        memset(&source, 0, sizeof(source));
        RUDA_EXPECT(ruda_test_native_build_panic_leaf(&leaf, leaf_symbol));
        RUDA_EXPECT(ruda_test_native_build_array_never_entry(
            &entry, entry_symbol, leaf_symbol, indices[index]
        ));
        RUDA_EXPECT(ruda_mir_verify(&leaf) == RUDA_MIR_VERIFY_OK);
        RUDA_EXPECT(ruda_mir_verify(&entry) == RUDA_MIR_VERIFY_OK);
        functions[0] = &leaf;
        functions[1] = &entry;
        RUDA_EXPECT(ruda_mir_lower_bytecode_program(
            functions, 2, entry_symbol, ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(), &bytecode
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        RUDA_EXPECT(ruda_bytecode_execution_init(
            &execution, ruda_system_allocator(),
            ruda_bytecode_execution_default_options()
        ));
        RUDA_EXPECT(ruda_bytecode_panic_init(
            &panic, ruda_system_allocator()
        ));
        run_options = ruda_bytecode_run_default_options();
        run_options.execution = &execution;
        run_options.panic = &panic;
        memset(&result, 0, sizeof(result));
        RUDA_EXPECT(ruda_bytecode_run_program(
            &bytecode, NULL, 0, ruda_system_allocator(), run_options, &result
        ) == RUDA_BYTECODE_RUN_PANIC);
        RUDA_EXPECT(panic.code != NULL
            && strcmp(panic.code, indices[index] < 0 ? "G4002" : "G4000") == 0);
        memset(&native_program, 0, sizeof(native_program));
        native_program.functions = functions;
        native_program.function_count = 2;
        native_program.entry_symbol = entry_symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &native_program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &source
        ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&source, outputs[index], 1);
#endif
        ruda_owned_bytes_dispose(&source);
        ruda_bytecode_panic_dispose(&panic);
        ruda_bytecode_execution_dispose(&execution);
        ruda_bytecode_program_dispose(&bytecode);
        ruda_mir_function_dispose(&entry);
        ruda_mir_function_dispose(&leaf);
    }
}

static bool ruda_test_native_build_never_entry(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId callee
) {
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirTerminator terminator;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_NEVER, ruda_mir_function_default_options())
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_call(function, callee, RUDA_TYPE_NEVER, NULL, 0,
            &call)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    terminator = ruda_test_native_terminator(RUDA_MIR_CALL_NEVER,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 210);
    terminator.call = call;
    if (!ruda_mir_set_terminator(function, block, terminator)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_native_strings_and_panic(void) {
    static const unsigned char value[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    const RudaSymbolId return_symbol = {{0x40}};
    const RudaSymbolId equal_symbol = {{0x41}};
    const RudaSymbolId leaf_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    RudaMirFunction returned;
    RudaMirFunction equal;
    RudaMirFunction leaf;
    RudaMirFunction entry;
    const RudaMirFunction *one_function[1];
    const RudaMirFunction *ordered[2];
    const RudaMirFunction *permuted[2];
    RudaNativeCProgram native_program;
    RudaOwnedBytes first;
    RudaOwnedBytes second;
    RudaBytecodeFunction bytecode;
    RudaBytecodeProgram program;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t failure_index = 0;
    bool reached_success = false;

    memset(&returned, 0, sizeof(returned));
    memset(&equal, 0, sizeof(equal));
    memset(&leaf, 0, sizeof(leaf));
    memset(&entry, 0, sizeof(entry));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&program, 0, sizeof(program));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(ruda_test_native_build_string_return(&returned, return_symbol));
    RUDA_EXPECT(ruda_test_native_build_string_equal(&equal, equal_symbol));
    RUDA_EXPECT(ruda_test_native_build_panic_leaf(&leaf, leaf_symbol));
    RUDA_EXPECT(ruda_test_native_build_never_entry(&entry, entry_symbol,
        leaf_symbol));
    RUDA_EXPECT(ruda_mir_verify(&returned) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&equal) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&leaf) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&entry) == RUDA_MIR_VERIFY_OK);

    one_function[0] = &returned;
    native_program.functions = one_function;
    native_program.function_count = 1;
    native_program.entry_symbol = return_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &first)
        == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &second)
        == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(first.length != 0 && first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
    RUDA_EXPECT(ruda_test_native_contains(&first, "ruda_g_string"));
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first, "ok:string:6:700021e29883\n", 0);
#endif
    ruda_owned_bytes_dispose(&second);
    ruda_owned_bytes_dispose(&first);

    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&returned,
        ruda_system_allocator(), ruda_mir_bytecode_lower_default_options(),
        &bytecode) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()));
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = INT64_MIN;
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(&bytecode, NULL, 0, ruda_system_allocator(),
        run_options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_STRING
        && ruda_bytecode_execution_string_bytes(&execution, result, &bytes, &length)
        && length == sizeof(value) && memcmp(bytes, value, sizeof(value)) == 0);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_function_dispose(&bytecode);

    one_function[0] = &equal;
    native_program.entry_symbol = equal_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &first)
        == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(ruda_test_native_contains(&first, "ruda_g_string_equal"));
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first, "ok:bool:true\n", 0);
#endif
    ruda_owned_bytes_dispose(&first);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&equal, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &bytecode)
        == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()));
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = INT64_MIN;
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(&bytecode, NULL, 0, ruda_system_allocator(),
        run_options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL && result.as.boolean);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_function_dispose(&bytecode);

    ordered[0] = &leaf;
    ordered[1] = &entry;
    permuted[0] = &entry;
    permuted[1] = &leaf;
    native_program.functions = ordered;
    native_program.function_count = 2;
    native_program.entry_symbol = entry_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &first)
        == RUDA_NATIVE_C_EMIT_OK);
    native_program.functions = permuted;
    RUDA_EXPECT(ruda_native_c_emit_program(&native_program,
        ruda_system_allocator(), ruda_native_c_emit_default_options(), &second)
        == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first,
        "panic:G4000:10000000000000000000000000000000:201:202:6:700021e29883:2:"
        "10000000000000000000000000000000:201:202:"
        "20000000000000000000000000000000:210:211\n", 1);
#endif
    ruda_owned_bytes_dispose(&second);
    ruda_owned_bytes_dispose(&first);

    RUDA_EXPECT(ruda_mir_lower_bytecode_program(permuted, 2, entry_symbol,
        ruda_system_allocator(), ruda_mir_bytecode_program_lower_default_options(),
        &program) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()));
    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = 1234;
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), run_options, &result) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0
        && panic.message.length == sizeof(value)
        && memcmp(panic.message.data, value, sizeof(value)) == 0
        && memcmp(&panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && panic.span.start == 201 && panic.span.end == 202
        && panic.frames.length == 2);
    {
        const RudaBytecodePanicFrame *leaf_frame = ruda_bytecode_panic_frame(
            &panic, 0
        );
        const RudaBytecodePanicFrame *entry_frame = ruda_bytecode_panic_frame(
            &panic, 1
        );
        RUDA_EXPECT(leaf_frame != NULL && entry_frame != NULL
            && memcmp(&leaf_frame->symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
            && leaf_frame->call_span.start == 201 && leaf_frame->call_span.end == 202
            && memcmp(&entry_frame->symbol, &entry_symbol, sizeof(entry_symbol)) == 0
            && entry_frame->call_span.start == 210
            && entry_frame->call_span.end == 211);
    }
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_program_dispose(&program);

    {
        RudaNativeCEmitOptions options = ruda_native_c_emit_default_options();
        options.max_functions = 1;
        ruda_test_native_emit(ordered, 2, entry_symbol, options,
            RUDA_NATIVE_C_EMIT_LIMIT_REACHED);
        options = ruda_native_c_emit_default_options();
        options.max_generated_bytes = 1;
        ruda_test_native_emit(ordered, 2, entry_symbol, options,
            RUDA_NATIVE_C_EMIT_LIMIT_REACHED);
    }
    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes output;
        RudaNativeCEmitStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = ruda_native_c_emit_program(&native_program,
            ruda_fail_allocator(&state), ruda_native_c_emit_default_options(),
            &output);
        RUDA_EXPECT(status == RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED
            || status == RUDA_NATIVE_C_EMIT_OK);
        reached_success = reached_success || status == RUDA_NATIVE_C_EMIT_OK;
        ruda_owned_bytes_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_mir_function_dispose(&entry);
    ruda_mir_function_dispose(&leaf);
    ruda_mir_function_dispose(&equal);
    ruda_mir_function_dispose(&returned);
}

void ruda_test_native_c_emitter(void) {
    const RudaSymbolId leaf_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    RudaMirFunction leaf;
    RudaMirFunction entry;
    const RudaMirFunction *ordered[2];
    const RudaMirFunction *permuted[2];
    RudaNativeCProgram program;
    RudaNativeCEmitOptions options = ruda_native_c_emit_default_options();
    RudaOwnedBytes first;
    RudaOwnedBytes second;
    size_t failure_index = 0;
    bool reached_success = false;

    ruda_test_native_scalar_differential();
    ruda_test_native_f64_differential();
    ruda_test_native_f64_direct_call();
    ruda_test_native_f64_source_differential();
    ruda_test_native_strings_and_panic();
    ruda_test_native_array_rhs_ordering();
    ruda_test_native_arrays_differential();
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    RUDA_EXPECT(ruda_test_native_build_leaf(&leaf, leaf_symbol));
    RUDA_EXPECT(ruda_test_native_build_entry(&entry, entry_symbol, leaf_symbol,
        true));
    RUDA_EXPECT(ruda_mir_verify(&leaf) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&entry) == RUDA_MIR_VERIFY_OK);
    ordered[0] = &leaf;
    ordered[1] = &entry;
    permuted[0] = &entry;
    permuted[1] = &leaf;
    ruda_test_native_vm_program(permuted, entry_symbol, 42);
    memset(&program, 0, sizeof(program));
    program.functions = ordered;
    program.function_count = 2;
    program.entry_symbol = entry_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
        options, &first) == RUDA_NATIVE_C_EMIT_OK);
    program.functions = permuted;
    RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
        options, &second) == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(first.length != 0 && first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
    RUDA_EXPECT(ruda_test_native_contains(&first, "#include <stdint.h>"));
    RUDA_EXPECT(ruda_test_native_contains(&first, "ruda_g_b_"));
    RUDA_EXPECT(ruda_test_native_contains(&first, "ruda_g_f_"));
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first, "ok:i64:42\n", 0);
#endif
    ruda_owned_bytes_dispose(&second);
    ruda_owned_bytes_dispose(&first);

    {
        RudaMirFunction overflow;
        const RudaMirFunction *overflow_functions[1];
        RudaOwnedBytes overflow_source;
        memset(&overflow_source, 0, sizeof(overflow_source));
        RUDA_EXPECT(ruda_test_native_build_overflow(&overflow, leaf_symbol));
        ruda_test_native_vm_overflow(&overflow);
        overflow_functions[0] = &overflow;
        program.functions = overflow_functions;
        program.function_count = 1;
        program.entry_symbol = leaf_symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &overflow_source)
            == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&overflow_source, "trap:1:32:33\n", 1);
#endif
        ruda_owned_bytes_dispose(&overflow_source);
        ruda_mir_function_dispose(&overflow);
        program.functions = permuted;
        program.function_count = 2;
        program.entry_symbol = entry_symbol;
    }

    {
        RudaMirFunction false_entry;
        const RudaMirFunction *false_functions[2];
        RudaOwnedBytes false_source;
        memset(&false_source, 0, sizeof(false_source));
        RUDA_EXPECT(ruda_test_native_build_entry(&false_entry, entry_symbol,
            leaf_symbol, false));
        false_functions[0] = &false_entry;
        false_functions[1] = &leaf;
        ruda_test_native_vm_program(false_functions, entry_symbol, 0);
        program.functions = false_functions;
        program.function_count = 2;
        program.entry_symbol = entry_symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &false_source)
            == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&false_source, "ok:i64:0\n", 0);
#endif
        ruda_owned_bytes_dispose(&false_source);
        ruda_mir_function_dispose(&false_entry);
        program.functions = permuted;
        program.function_count = 2;
        program.entry_symbol = entry_symbol;
    }

    options.max_functions = 1;
    ruda_test_native_emit(ordered, 2, entry_symbol, options,
        RUDA_NATIVE_C_EMIT_LIMIT_REACHED);
    options = ruda_native_c_emit_default_options();
    options.max_generated_bytes = 1;
    ruda_test_native_emit(ordered, 2, entry_symbol, options,
        RUDA_NATIVE_C_EMIT_LIMIT_REACHED);
    options = ruda_native_c_emit_default_options();
    ruda_test_native_emit(NULL, 0, entry_symbol, options,
        RUDA_NATIVE_C_EMIT_INVALID_ARGUMENT);
    ruda_test_native_emit(permuted, 1, entry_symbol, options,
        RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL);
    {
        RudaMirInstruction *malformed = ruda_vector_at(
            &((RudaMirBlock *)ruda_vector_at(&entry.blocks, 0))->instructions, 0
        );
        RUDA_EXPECT(malformed != NULL);
        if (malformed != NULL) {
            malformed->kind = (RudaMirInstructionKind)999;
            ruda_test_native_emit(permuted, 2, entry_symbol, options,
                RUDA_NATIVE_C_EMIT_INVALID_MIR);
            malformed->kind = RUDA_MIR_CONSTANT_BOOL;
        }
    }
    {
        RudaMirCall *call = ruda_vector_at(&entry.calls, 0);
        RUDA_EXPECT(call != NULL);
        if (call != NULL) {
            call->callee = entry_symbol;
            ruda_test_native_emit(permuted, 2, entry_symbol, options,
                RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL);
            call->callee = leaf_symbol;
        }
    }

    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes output;
        RudaNativeCEmitStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = ruda_native_c_emit_program(&program, ruda_fail_allocator(&state),
            ruda_native_c_emit_default_options(), &output);
        RUDA_EXPECT(status == RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED
            || status == RUDA_NATIVE_C_EMIT_OK);
        if (status == RUDA_NATIVE_C_EMIT_OK) {
            reached_success = true;
        }
        ruda_owned_bytes_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_mir_function_dispose(&entry);
    ruda_mir_function_dispose(&leaf);
}
