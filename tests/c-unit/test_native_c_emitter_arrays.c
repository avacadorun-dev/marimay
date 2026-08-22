#include "test_native_c_emitter_support.h"

#include <inttypes.h>
#include <limits.h>
#include <string.h>

static bool mmay_test_native_build_generic_array(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    const MarimayTypeId array_type = MMAY_TYPE_FIRST_DYNAMIC + 130;
    MarimayMirRegister one = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister two = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister three = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister fetched = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister length = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister pushed = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId construction = MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirArrayArgument arguments[2];
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_I64, mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_I64, &one)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &two)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &three)
        || !mmay_mir_add_register(function, array_type, &array_value)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &index)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &fetched)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &length)
        || !mmay_mir_add_register(function, array_type, &pushed)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &result)
        || !mmay_mir_add_array(function, array_type, MMAY_TYPE_I64, &array)
        || !mmay_mir_add_block(function, &block)) {
        goto failure;
    }
    arguments[0].register_id = one;
    arguments[0].type = MMAY_TYPE_I64;
    arguments[1].register_id = two;
    arguments[1].type = MMAY_TYPE_I64;
    if (!mmay_mir_add_array_construction(
            function, array, arguments, 2, &construction
        )) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, one, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 1, false, 1300
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, two, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 2, false, 1301
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_NEW, array_value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 1302
    );
    instruction.array = array;
    instruction.array_construction = construction;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, three, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 3, false, 1303
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_PUSH, pushed, array_value, three,
        0, false, 1304
    );
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, index, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 2, false, 1305
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_GET, fetched, array_value, index,
        0, false, 1306
    );
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_LENGTH, length, array_value, MMAY_MIR_NO_REGISTER,
        0, false, 1307
    );
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ADD_CHECKED_I64, result, fetched, length,
        0, false, 1308
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block, mmay_test_native_terminator(
                MMAY_MIR_RETURN, result, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 1309
            )
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static bool mmay_test_native_build_array(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    int64_t access_index
) {
    const MarimayTypeId array_type = MMAY_TYPE_FIRST_DYNAMIC + 100;
    MarimayMirRegister first = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister second = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister length = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister checked = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister replacement = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId construction = MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirArrayArgument arguments[2];
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_F64, mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &first)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &second)
        || !mmay_mir_add_register(function, array_type, &array_value)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &length)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &index)
        || !mmay_mir_add_register(function, array_type, &checked)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &replacement)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &result)
        || !mmay_mir_add_array(function, array_type, MMAY_TYPE_F64, &array)
        || !mmay_mir_add_block(function, &block)) {
        goto failure;
    }
    arguments[0].register_id = first;
    arguments[0].type = MMAY_TYPE_F64;
    arguments[1].register_id = second;
    arguments[1].type = MMAY_TYPE_F64;
    if (!mmay_mir_add_array_construction(
            function, array, arguments, 2, &construction
        )) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_F64, first, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 800
    );
    instruction.f64_bits = UINT64_C(0x3ff8000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_F64, second, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 801
    );
    instruction.f64_bits = UINT64_C(0x4004000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_NEW, array_value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 802
    );
    instruction.array = array;
    instruction.array_construction = construction;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_LENGTH, length, array_value, MMAY_MIR_NO_REGISTER,
        0, false, 803
    );
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, index, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, access_index, false, 804
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_REQUIRE_INDEX, checked, array_value, index,
        0, false, 805
    );
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_F64, replacement, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 806
    );
    instruction.f64_bits = UINT64_C(0x4022000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_SET, result, checked, index, 0, false, 805
    );
    instruction.third = replacement;
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_GET, result, array_value, index, 0, false, 807
    );
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block, mmay_test_native_terminator(
                MMAY_MIR_RETURN, result, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 808
            )
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static bool mmay_test_native_build_nested_array(
    MarimayMirFunction *function,
    MarimaySymbolId symbol
) {
    const MarimayTypeId inner_type = MMAY_TYPE_FIRST_DYNAMIC + 110;
    const MarimayTypeId outer_type = MMAY_TYPE_FIRST_DYNAMIC + 111;
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister inner = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister outer = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister fetched = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirArrayId inner_array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayId outer_array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId inner_construction =
        MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirArrayConstructionId outer_construction =
        MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirArrayArgument argument;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_F64, mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &value)
        || !mmay_mir_add_register(function, inner_type, &inner)
        || !mmay_mir_add_register(function, outer_type, &outer)
        || !mmay_mir_add_register(function, inner_type, &fetched)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &index)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &result)
        || !mmay_mir_add_array(
            function, inner_type, MMAY_TYPE_F64, &inner_array
        ) || !mmay_mir_add_array(
            function, outer_type, inner_type, &outer_array
        ) || !mmay_mir_add_block(function, &block)) {
        goto failure;
    }
    argument.register_id = value;
    argument.type = MMAY_TYPE_F64;
    if (!mmay_mir_add_array_construction(
            function, inner_array, &argument, 1, &inner_construction
        )) {
        goto failure;
    }
    argument.register_id = inner;
    argument.type = inner_type;
    if (!mmay_mir_add_array_construction(
            function, outer_array, &argument, 1, &outer_construction
        )) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_F64, value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 820
    );
    instruction.f64_bits = UINT64_C(0x400c000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_NEW, inner, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 821
    );
    instruction.array = inner_array;
    instruction.array_construction = inner_construction;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_NEW, outer, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 822
    );
    instruction.array = outer_array;
    instruction.array_construction = outer_construction;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, index, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 823
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_GET, fetched, outer, index, 0, false, 824
    );
    instruction.array = outer_array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_GET, result, fetched, index, 0, false, 825
    );
    instruction.array = inner_array;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block, mmay_test_native_terminator(
                MMAY_MIR_RETURN, result, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 826
            )
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static bool mmay_test_native_build_array_call(
    MarimayMirFunction *leaf,
    MarimayMirFunction *entry,
    MarimaySymbolId leaf_symbol,
    MarimaySymbolId entry_symbol
) {
    const MarimayTypeId array_type = MMAY_TYPE_FIRST_DYNAMIC + 120;
    MarimayMirRegister leaf_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister leaf_array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirArrayId leaf_array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId leaf_construction =
        MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirArrayArgument argument;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirRegister returned = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister garbage_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister garbage = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirArrayId entry_array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId garbage_construction =
        MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    memset(leaf, 0, sizeof(*leaf));
    memset(entry, 0, sizeof(*entry));
    if (!mmay_mir_function_init(
            leaf, mmay_system_allocator(), leaf_symbol, NULL, 0, array_type,
            mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(leaf, MMAY_TYPE_F64, &leaf_value)
        || !mmay_mir_add_register(leaf, array_type, &leaf_array_value)
        || !mmay_mir_add_array(
            leaf, array_type, MMAY_TYPE_F64, &leaf_array
        ) || !mmay_mir_add_block(leaf, &block)) {
        goto failure;
    }
    argument.register_id = leaf_value;
    argument.type = MMAY_TYPE_F64;
    if (!mmay_mir_add_array_construction(
            leaf, leaf_array, &argument, 1, &leaf_construction
        )) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_F64, leaf_value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 840
    );
    instruction.f64_bits = UINT64_C(0x4014000000000000);
    if (!mmay_mir_append_instruction(leaf, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_NEW, leaf_array_value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 841
    );
    instruction.array = leaf_array;
    instruction.array_construction = leaf_construction;
    if (!mmay_mir_append_instruction(leaf, block, &instruction)
        || !mmay_mir_set_terminator(
            leaf, block, mmay_test_native_terminator(
                MMAY_MIR_RETURN, leaf_array_value, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 842
            )
        )) {
        goto failure;
    }
    block = MMAY_MIR_NO_BLOCK;
    if (!mmay_mir_function_init(
            entry, mmay_system_allocator(), entry_symbol, NULL, 0,
            MMAY_TYPE_F64, mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(entry, array_type, &returned)
        || !mmay_mir_add_register(entry, MMAY_TYPE_F64, &garbage_value)
        || !mmay_mir_add_register(entry, array_type, &garbage)
        || !mmay_mir_add_register(entry, MMAY_TYPE_I64, &index)
        || !mmay_mir_add_register(entry, MMAY_TYPE_F64, &result)
        || !mmay_mir_add_array(
            entry, array_type, MMAY_TYPE_F64, &entry_array
        ) || !mmay_mir_add_call(
            entry, leaf_symbol, array_type, NULL, 0, &call
        ) || !mmay_mir_add_block(entry, &block)) {
        goto failure;
    }
    argument.register_id = garbage_value;
    argument.type = MMAY_TYPE_F64;
    if (!mmay_mir_add_array_construction(
            entry, entry_array, &argument, 1, &garbage_construction
        )) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CALL, returned, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 850
    );
    instruction.call = call;
    if (!mmay_mir_append_instruction(entry, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_F64, garbage_value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 851
    );
    instruction.f64_bits = UINT64_C(0x401c000000000000);
    if (!mmay_mir_append_instruction(entry, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_NEW, garbage, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 852
    );
    instruction.array = entry_array;
    instruction.array_construction = garbage_construction;
    if (!mmay_mir_append_instruction(entry, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, index, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 853
    );
    if (!mmay_mir_append_instruction(entry, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_GET, result, returned, index, 0, false, 854
    );
    instruction.array = entry_array;
    if (!mmay_mir_append_instruction(entry, block, &instruction)
        || !mmay_mir_set_terminator(
            entry, block, mmay_test_native_terminator(
                MMAY_MIR_RETURN, result, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 855
            )
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(entry);
    mmay_mir_function_dispose(leaf);
    return false;
}

static bool mmay_test_native_build_array_never_entry(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimaySymbolId callee,
    int64_t index_value
) {
    const MarimayTypeId array_type = MMAY_TYPE_FIRST_DYNAMIC + 130;
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister checked = MMAY_MIR_NO_REGISTER;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId construction = MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirArrayArgument argument;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirTerminator terminator;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_NEVER, mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &value)
        || !mmay_mir_add_register(function, array_type, &array_value)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &index)
        || !mmay_mir_add_register(function, array_type, &checked)
        || !mmay_mir_add_array(function, array_type, MMAY_TYPE_F64, &array)
        || !mmay_mir_add_call(
            function, callee, MMAY_TYPE_NEVER, NULL, 0, &call
        ) || !mmay_mir_add_block(function, &block)) {
        goto failure;
    }
    argument.register_id = value;
    argument.type = MMAY_TYPE_F64;
    if (!mmay_mir_add_array_construction(
            function, array, &argument, 1, &construction
        )) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_F64, value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 860
    );
    instruction.f64_bits = UINT64_C(0x3ff0000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_NEW, array_value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 861
    );
    instruction.array = array;
    instruction.array_construction = construction;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, index, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, index_value, false, 862
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_REQUIRE_INDEX, checked, array_value, index,
        0, false, 863
    );
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    terminator = mmay_test_native_terminator(
        MMAY_MIR_CALL_NEVER, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_BLOCK,
        MMAY_MIR_NO_BLOCK, 866
    );
    terminator.call = call;
    if (!mmay_mir_set_terminator(function, block, terminator)) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}


static void mmay_test_native_arrays_differential(void) {
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
    const MarimaySymbolId symbol = {{0x71}};
    size_t case_index = 0;
    for (case_index = 0;
        case_index < sizeof(indices) / sizeof(indices[0]);
        case_index += 1) {
        MarimayMirFunction function;
        MarimayMirExecution mir_execution;
        MarimayMirInterpretOptions mir_options;
        MarimayMirPanic mir_panic;
        MarimayMirValue mir_result;
        MarimayBytecodeFunction lowered;
        MarimayBytecodeFunction decoded;
        MarimayBytecodeExecution vm_execution;
        MarimayBytecodeRunOptions vm_options;
        MarimayBytecodePanic vm_panic;
        MarimayBytecodeValue vm_result;
        MarimayOwnedBytes encoded;
        MarimayOwnedBytes source;
        MarimayNativeCProgram program;
        const MarimayMirFunction *functions[1];
        memset(&function, 0, sizeof(function));
        memset(&mir_execution, 0, sizeof(mir_execution));
        memset(&mir_panic, 0, sizeof(mir_panic));
        memset(&lowered, 0, sizeof(lowered));
        memset(&decoded, 0, sizeof(decoded));
        memset(&vm_execution, 0, sizeof(vm_execution));
        memset(&vm_panic, 0, sizeof(vm_panic));
        memset(&encoded, 0, sizeof(encoded));
        memset(&source, 0, sizeof(source));
        MMAY_EXPECT(mmay_test_native_build_array(
            &function, symbol, indices[case_index]
        ));
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_mir_execution_init(
            &mir_execution, mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        mir_options = mmay_mir_interpret_default_options();
        mir_options.execution = &mir_execution;
        mir_options.panic = &mir_panic;
        memset(&mir_result, 0, sizeof(mir_result));
        if (indices[case_index] == 1) {
            MMAY_EXPECT(mmay_mir_interpret(
                &function, NULL, 0, mmay_system_allocator(), mir_options,
                &mir_result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(mir_result.type == MMAY_TYPE_F64
                && mir_result.as.f64_bits == UINT64_C(0x4022000000000000));
        } else {
            MMAY_EXPECT(mmay_mir_interpret(
                &function, NULL, 0, mmay_system_allocator(), mir_options,
                &mir_result
            ) == MMAY_MIR_INTERPRET_PANIC);
            MMAY_EXPECT(mir_panic.code != NULL
                && strcmp(mir_panic.code, "G4002") == 0
                && mir_panic.span.start == 805 && mir_panic.span.end == 806
                && memcmp(&mir_panic.symbol, &symbol, sizeof(symbol)) == 0);
        }
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(
            &function, mmay_system_allocator(),
            mmay_mir_bytecode_lower_default_options(), &lowered
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        MMAY_EXPECT(mmay_bytecode_encode_function(
            &lowered, mmay_system_allocator(), &encoded
        ) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_decode_function(
            encoded.data, encoded.length, mmay_system_allocator(),
            mmay_bytecode_function_default_options(), &decoded
        ) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_execution_init(
            &vm_execution, mmay_system_allocator(),
            mmay_bytecode_execution_default_options()
        ));
        MMAY_EXPECT(mmay_bytecode_panic_init(
            &vm_panic, mmay_system_allocator()
        ));
        vm_options = mmay_bytecode_run_default_options();
        vm_options.execution = &vm_execution;
        vm_options.panic = &vm_panic;
        memset(&vm_result, 0, sizeof(vm_result));
        if (indices[case_index] == 1) {
            MMAY_EXPECT(mmay_bytecode_run(
                &decoded, NULL, 0, mmay_system_allocator(), vm_options,
                &vm_result
            ) == MMAY_BYTECODE_RUN_OK);
            MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_F64
                && vm_result.as.f64_bits == UINT64_C(0x4022000000000000));
        } else {
            MMAY_EXPECT(mmay_bytecode_run(
                &decoded, NULL, 0, mmay_system_allocator(), vm_options,
                &vm_result
            ) == MMAY_BYTECODE_RUN_PANIC);
            MMAY_EXPECT(vm_panic.code != NULL
                && strcmp(vm_panic.code, "G4002") == 0
                && vm_panic.span.start == 805 && vm_panic.span.end == 806
                && vm_panic.frames.length == 1);
            MMAY_EXPECT(vm_panic.message.length == mir_panic.message.length);
            if (vm_panic.message.length == mir_panic.message.length) {
                MMAY_EXPECT(memcmp(
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
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &source
        ) == MMAY_NATIVE_C_EMIT_OK);
        MMAY_EXPECT(mmay_test_native_contains(
            &source, "mmay_managed_array_new"
        ));
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(
            &source, outputs[case_index], indices[case_index] == 1 ? 0 : 1
        );
        if (indices[case_index] == 1) {
            mmay_test_native_compile_and_run_configured(
                &source, "trap:5:0:0\n", 1,
                "-DMMAY_G_FAIL_ALLOCATION_AT=0"
            );
            mmay_test_native_compile_and_run_configured(
                &source, "trap:5:0:0\n", 1,
                "-DMMAY_G_FAIL_ALLOCATION_AT=1"
            );
        }
#endif
        mmay_owned_bytes_dispose(&source);
        mmay_bytecode_panic_dispose(&vm_panic);
        mmay_bytecode_execution_dispose(&vm_execution);
        mmay_bytecode_function_dispose(&decoded);
        mmay_owned_bytes_dispose(&encoded);
        mmay_bytecode_function_dispose(&lowered);
        mmay_mir_panic_dispose(&mir_panic);
        mmay_mir_execution_dispose(&mir_execution);
        mmay_mir_function_dispose(&function);
    }
    {
        const MarimaySymbolId nested_symbol = {{0x72}};
        MarimayMirFunction function;
        MarimayMirExecution mir_execution;
        MarimayMirInterpretOptions mir_options;
        MarimayMirValue mir_result;
        MarimayBytecodeFunction lowered;
        MarimayBytecodeExecution vm_execution;
        MarimayBytecodeRunOptions vm_options;
        MarimayBytecodeValue vm_result;
        MarimayOwnedBytes source;
        MarimayNativeCProgram program;
        const MarimayMirFunction *functions[1];
        memset(&function, 0, sizeof(function));
        memset(&mir_execution, 0, sizeof(mir_execution));
        memset(&lowered, 0, sizeof(lowered));
        memset(&vm_execution, 0, sizeof(vm_execution));
        memset(&source, 0, sizeof(source));
        MMAY_EXPECT(mmay_test_native_build_nested_array(
            &function, nested_symbol
        ));
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_mir_execution_init(
            &mir_execution, mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        mir_options = mmay_mir_interpret_default_options();
        mir_options.execution = &mir_execution;
        memset(&mir_result, 0, sizeof(mir_result));
        MMAY_EXPECT(mmay_mir_interpret(
            &function, NULL, 0, mmay_system_allocator(), mir_options,
            &mir_result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mir_result.type == MMAY_TYPE_F64
            && mir_result.as.f64_bits == UINT64_C(0x400c000000000000));
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(
            &function, mmay_system_allocator(),
            mmay_mir_bytecode_lower_default_options(), &lowered
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        {
            MarimayBytecodeExecutionOptions execution_options =
                mmay_bytecode_execution_default_options();
            execution_options.managed_heap.collect_before_allocation = true;
            MMAY_EXPECT(mmay_bytecode_execution_init(
                &vm_execution, mmay_system_allocator(), execution_options
            ));
        }
        vm_options = mmay_bytecode_run_default_options();
        vm_options.execution = &vm_execution;
        memset(&vm_result, 0, sizeof(vm_result));
        MMAY_EXPECT(mmay_bytecode_run(
            &lowered, NULL, 0, mmay_system_allocator(), vm_options,
            &vm_result
        ) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_F64
            && vm_result.as.f64_bits == mir_result.as.f64_bits);
        functions[0] = &function;
        memset(&program, 0, sizeof(program));
        program.functions = functions;
        program.function_count = 1;
        program.entry_symbol = nested_symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &source
        ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(
            &source, "ok:f64:400c000000000000\n", 0
        );
        mmay_test_native_compile_and_run_configured(
            &source, "trap:6:0:0\n", 1,
            "-DMMAY_G_MAX_OBJECTS=1"
        );
#endif
        mmay_owned_bytes_dispose(&source);
        mmay_bytecode_execution_dispose(&vm_execution);
        mmay_bytecode_function_dispose(&lowered);
        mmay_mir_execution_dispose(&mir_execution);
        mmay_mir_function_dispose(&function);
    }
    {
        const MarimaySymbolId leaf_symbol = {{0x73}};
        const MarimaySymbolId entry_symbol = {{0x74}};
        MarimayMirFunction leaf;
        MarimayMirFunction entry;
        const MarimayMirFunction *functions[2];
        MarimayBytecodeProgram bytecode;
        MarimayBytecodeExecution execution;
        MarimayBytecodeExecutionOptions execution_options;
        MarimayBytecodeRunOptions run_options;
        MarimayBytecodeValue result;
        MarimayNativeCProgram native_program;
        MarimayOwnedBytes source;
        memset(&leaf, 0, sizeof(leaf));
        memset(&entry, 0, sizeof(entry));
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&execution, 0, sizeof(execution));
        memset(&source, 0, sizeof(source));
        MMAY_EXPECT(mmay_test_native_build_array_call(
            &leaf, &entry, leaf_symbol, entry_symbol
        ));
        MMAY_EXPECT(mmay_mir_verify(&leaf) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_mir_verify(&entry) == MMAY_MIR_VERIFY_OK);
        functions[0] = &leaf;
        functions[1] = &entry;
        MMAY_EXPECT(mmay_mir_lower_bytecode_program(
            functions, 2, entry_symbol, mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(), &bytecode
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        execution_options = mmay_bytecode_execution_default_options();
        execution_options.managed_heap.collect_before_allocation = true;
        MMAY_EXPECT(mmay_bytecode_execution_init(
            &execution, mmay_system_allocator(), execution_options
        ));
        run_options = mmay_bytecode_run_default_options();
        run_options.execution = &execution;
        memset(&result, 0, sizeof(result));
        MMAY_EXPECT(mmay_bytecode_run_program(
            &bytecode, NULL, 0, mmay_system_allocator(), run_options, &result
        ) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_F64
            && result.as.f64_bits == UINT64_C(0x4014000000000000));
        memset(&native_program, 0, sizeof(native_program));
        native_program.functions = functions;
        native_program.function_count = 2;
        native_program.entry_symbol = entry_symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &native_program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &source
        ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(
            &source, "ok:f64:4014000000000000\n", 0
        );
#endif
        mmay_owned_bytes_dispose(&source);
        mmay_bytecode_execution_dispose(&execution);
        mmay_bytecode_program_dispose(&bytecode);
        mmay_mir_function_dispose(&entry);
        mmay_mir_function_dispose(&leaf);
    }
}


static void mmay_test_native_array_rhs_ordering(void) {
    const MarimaySymbolId leaf_symbol = {{0x75}};
    const MarimaySymbolId entry_symbol = {{0x76}};
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
        MarimayMirFunction leaf;
        MarimayMirFunction entry;
        const MarimayMirFunction *functions[2];
        MarimayBytecodeProgram bytecode;
        MarimayBytecodeExecution execution;
        MarimayBytecodePanic panic;
        MarimayBytecodeRunOptions run_options;
        MarimayBytecodeValue result;
        MarimayNativeCProgram native_program;
        MarimayOwnedBytes source;
        memset(&leaf, 0, sizeof(leaf));
        memset(&entry, 0, sizeof(entry));
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        memset(&source, 0, sizeof(source));
        MMAY_EXPECT(mmay_test_native_build_panic_leaf(&leaf, leaf_symbol));
        MMAY_EXPECT(mmay_test_native_build_array_never_entry(
            &entry, entry_symbol, leaf_symbol, indices[index]
        ));
        MMAY_EXPECT(mmay_mir_verify(&leaf) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_mir_verify(&entry) == MMAY_MIR_VERIFY_OK);
        functions[0] = &leaf;
        functions[1] = &entry;
        MMAY_EXPECT(mmay_mir_lower_bytecode_program(
            functions, 2, entry_symbol, mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(), &bytecode
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        MMAY_EXPECT(mmay_bytecode_execution_init(
            &execution, mmay_system_allocator(),
            mmay_bytecode_execution_default_options()
        ));
        MMAY_EXPECT(mmay_bytecode_panic_init(
            &panic, mmay_system_allocator()
        ));
        run_options = mmay_bytecode_run_default_options();
        run_options.execution = &execution;
        run_options.panic = &panic;
        memset(&result, 0, sizeof(result));
        MMAY_EXPECT(mmay_bytecode_run_program(
            &bytecode, NULL, 0, mmay_system_allocator(), run_options, &result
        ) == MMAY_BYTECODE_RUN_PANIC);
        MMAY_EXPECT(panic.code != NULL
            && strcmp(panic.code, indices[index] < 0 ? "G4002" : "G4000") == 0);
        memset(&native_program, 0, sizeof(native_program));
        native_program.functions = functions;
        native_program.function_count = 2;
        native_program.entry_symbol = entry_symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &native_program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &source
        ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&source, outputs[index], 1);
#endif
        mmay_owned_bytes_dispose(&source);
        mmay_bytecode_panic_dispose(&panic);
        mmay_bytecode_execution_dispose(&execution);
        mmay_bytecode_program_dispose(&bytecode);
        mmay_mir_function_dispose(&entry);
        mmay_mir_function_dispose(&leaf);
    }
}

static void mmay_test_native_generic_array(void) {
    const MarimaySymbolId symbol = {{0x77}};
    MarimayMirFunction function;
    MarimayMirExecution mir_execution;
    MarimayMirInterpretOptions mir_options;
    MarimayMirValue mir_result;
    MarimayBytecodeFunction lowered;
    MarimayBytecodeExecution vm_execution;
    MarimayBytecodeExecutionOptions vm_execution_options;
    MarimayBytecodeRunOptions vm_options;
    MarimayBytecodeValue vm_result;
    MarimayNativeCProgram program;
    MarimayOwnedBytes source;
    const MarimayMirFunction *functions[1];
    memset(&function, 0, sizeof(function));
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&lowered, 0, sizeof(lowered));
    memset(&vm_execution, 0, sizeof(vm_execution));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&program, 0, sizeof(program));
    memset(&source, 0, sizeof(source));
    MMAY_EXPECT(mmay_test_native_build_generic_array(&function, symbol));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_execution_init(
        &mir_execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    mir_options = mmay_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(), mir_options, &mir_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mir_result.type == MMAY_TYPE_I64 && mir_result.as.i64 == 6);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(lowered.array_types.length == 1);
    MMAY_EXPECT(mmay_bytecode_verify(&lowered) == MMAY_BYTECODE_VERIFY_OK);
    vm_execution_options = mmay_bytecode_execution_default_options();
    vm_execution_options.managed_heap.collect_before_allocation = true;
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &vm_execution, mmay_system_allocator(), vm_execution_options
    ));
    vm_options = mmay_bytecode_run_default_options();
    vm_options.execution = &vm_execution;
    MMAY_EXPECT(mmay_bytecode_run(
        &lowered, NULL, 0, mmay_system_allocator(), vm_options, &vm_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_I64
        && vm_result.as.i64 == 6);
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeExecution failed_execution;
            MarimayBytecodeRunStatus status;
            memset(&failed_execution, 0, sizeof(failed_execution));
            mmay_fail_allocator_init(
                &state, mmay_system_allocator(), SIZE_MAX
            );
            MMAY_EXPECT(mmay_bytecode_execution_init(
                &failed_execution, mmay_fail_allocator(&state),
                vm_execution_options
            ));
            state.failure_index = state.attempts + failure_index;
            vm_options.execution = &failed_execution;
            status = mmay_bytecode_run(
                &lowered, NULL, 0, mmay_fail_allocator(&state),
                vm_options, &vm_result
            );
            if (status == MMAY_BYTECODE_RUN_OK) {
                reached_success = true;
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
            }
            mmay_bytecode_execution_dispose(&failed_execution);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
    }
    vm_options.execution = &vm_execution;
    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &source
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_test_native_contains(&source, "mmay_g_ad_"));
    MMAY_EXPECT(mmay_test_native_contains(
        &source, "mmay_managed_array_generated_push_raw64"
    ));
    MMAY_EXPECT(!mmay_test_native_contains(&source, "MarimayTypeId"));
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&source, "ok:i64:6\n", 0);
#endif
    mmay_owned_bytes_dispose(&source);
    mmay_bytecode_execution_dispose(&vm_execution);
    mmay_bytecode_function_dispose(&lowered);
    mmay_mir_execution_dispose(&mir_execution);
    mmay_mir_function_dispose(&function);
}


void mmay_test_native_arrays(void) {
    mmay_test_native_generic_array();
    mmay_test_native_array_rhs_ordering();
    mmay_test_native_arrays_differential();
}
