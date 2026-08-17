#include "test_native_c_emitter_support.h"

#include <inttypes.h>
#include <limits.h>
#include <string.h>

static bool ruda_test_native_build_generic_array(
    RudaMirFunction *function,
    RudaSymbolId symbol
) {
    const RudaTypeId array_type = RUDA_TYPE_FIRST_DYNAMIC + 130;
    RudaMirRegister one = RUDA_MIR_NO_REGISTER;
    RudaMirRegister two = RUDA_MIR_NO_REGISTER;
    RudaMirRegister three = RUDA_MIR_NO_REGISTER;
    RudaMirRegister array_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister fetched = RUDA_MIR_NO_REGISTER;
    RudaMirRegister length = RUDA_MIR_NO_REGISTER;
    RudaMirRegister pushed = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId construction = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirArrayArgument arguments[2];
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0,
            RUDA_TYPE_I64, ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_I64, &one)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &two)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &three)
        || !ruda_mir_add_register(function, array_type, &array_value)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &index)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &fetched)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &length)
        || !ruda_mir_add_register(function, array_type, &pushed)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &result)
        || !ruda_mir_add_array(function, array_type, RUDA_TYPE_I64, &array)
        || !ruda_mir_add_block(function, &block)) {
        goto failure;
    }
    arguments[0].register_id = one;
    arguments[0].type = RUDA_TYPE_I64;
    arguments[1].register_id = two;
    arguments[1].type = RUDA_TYPE_I64;
    if (!ruda_mir_add_array_construction(
            function, array, arguments, 2, &construction
        )) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, one, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 1, false, 1300
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, two, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 2, false, 1301
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_NEW, array_value, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 1302
    );
    instruction.array = array;
    instruction.array_construction = construction;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, three, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 3, false, 1303
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_PUSH, pushed, array_value, three,
        0, false, 1304
    );
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, index, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 2, false, 1305
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_GET, fetched, array_value, index,
        0, false, 1306
    );
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_LENGTH, length, array_value, RUDA_MIR_NO_REGISTER,
        0, false, 1307
    );
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ADD_CHECKED_I64, result, fetched, length,
        0, false, 1308
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block, ruda_test_native_terminator(
                RUDA_MIR_RETURN, result, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 1309
            )
        )) {
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

static void ruda_test_native_generic_array(void) {
    const RudaSymbolId symbol = {{0x77}};
    RudaMirFunction function;
    RudaMirExecution mir_execution;
    RudaMirInterpretOptions mir_options;
    RudaMirValue mir_result;
    RudaBytecodeFunction lowered;
    RudaBytecodeExecution vm_execution;
    RudaBytecodeExecutionOptions vm_execution_options;
    RudaBytecodeRunOptions vm_options;
    RudaBytecodeValue vm_result;
    RudaNativeCProgram program;
    RudaOwnedBytes source;
    const RudaMirFunction *functions[1];
    memset(&function, 0, sizeof(function));
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&lowered, 0, sizeof(lowered));
    memset(&vm_execution, 0, sizeof(vm_execution));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&program, 0, sizeof(program));
    memset(&source, 0, sizeof(source));
    RUDA_EXPECT(ruda_test_native_build_generic_array(&function, symbol));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_execution_init(
        &mir_execution, ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    mir_options = ruda_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(), mir_options, &mir_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(mir_result.type == RUDA_TYPE_I64 && mir_result.as.i64 == 6);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &function, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(lowered.array_types.length == 1);
    RUDA_EXPECT(ruda_bytecode_verify(&lowered) == RUDA_BYTECODE_VERIFY_OK);
    vm_execution_options = ruda_bytecode_execution_default_options();
    vm_execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &vm_execution, ruda_system_allocator(), vm_execution_options
    ));
    vm_options = ruda_bytecode_run_default_options();
    vm_options.execution = &vm_execution;
    RUDA_EXPECT(ruda_bytecode_run(
        &lowered, NULL, 0, ruda_system_allocator(), vm_options, &vm_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_I64
        && vm_result.as.i64 == 6);
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeExecution failed_execution;
            RudaBytecodeRunStatus status;
            memset(&failed_execution, 0, sizeof(failed_execution));
            ruda_fail_allocator_init(
                &state, ruda_system_allocator(), SIZE_MAX
            );
            RUDA_EXPECT(ruda_bytecode_execution_init(
                &failed_execution, ruda_fail_allocator(&state),
                vm_execution_options
            ));
            state.failure_index = state.attempts + failure_index;
            vm_options.execution = &failed_execution;
            status = ruda_bytecode_run(
                &lowered, NULL, 0, ruda_fail_allocator(&state),
                vm_options, &vm_result
            );
            if (status == RUDA_BYTECODE_RUN_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
            }
            ruda_bytecode_execution_dispose(&failed_execution);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
    }
    vm_options.execution = &vm_execution;
    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &source
    ) == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(ruda_test_native_contains(&source, "ruda_g_ad_"));
    RUDA_EXPECT(ruda_test_native_contains(&source, "ruda_managed_array_push"));
    RUDA_EXPECT(!ruda_test_native_contains(&source, "RudaTypeId"));
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&source, "ok:i64:6\n", 0);
#endif
    ruda_owned_bytes_dispose(&source);
    ruda_bytecode_execution_dispose(&vm_execution);
    ruda_bytecode_function_dispose(&lowered);
    ruda_mir_execution_dispose(&mir_execution);
    ruda_mir_function_dispose(&function);
}


void ruda_test_native_arrays(void) {
    ruda_test_native_generic_array();
    ruda_test_native_array_rhs_ordering();
    ruda_test_native_arrays_differential();
}
