#include "test_mir_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

#include "mmay/mir/range.h"

enum {
    MMAY_TEST_NATIVE_ACCESS_ARRAY_TYPE = MMAY_TYPE_FIRST_DYNAMIC + 151
};

typedef enum MarimayTestNativeAccessKind {
    MMAY_TEST_NATIVE_ACCESS_DIRECT,
    MMAY_TEST_NATIVE_ACCESS_CHECKED_PROOF,
    MMAY_TEST_NATIVE_ACCESS_STATIC_PROOF
} MarimayTestNativeAccessKind;

static size_t mmay_test_native_access_count(
    const MarimayOwnedBytes *bytes,
    const char *needle
) {
    size_t count = 0;
    size_t needle_length = needle != NULL ? strlen(needle) : 0;
    size_t index = 0;
    if (bytes == NULL || needle == NULL || needle_length == 0
        || needle_length > bytes->length) {
        return 0;
    }
    for (index = 0; index + needle_length <= bytes->length; index += 1) {
        if (memcmp(bytes->data + index, needle, needle_length) == 0) {
            count += 1;
            index += needle_length - 1;
        }
    }
    return count;
}

static bool mmay_test_native_access_build(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimayTestNativeAccessKind kind
) {
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister carrier = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister fetched = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister stored = MMAY_MIR_NO_REGISTER;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId construction = MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirArrayArgument argument;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_F64, mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &value)
        || !mmay_mir_add_register(
            function, MMAY_TEST_NATIVE_ACCESS_ARRAY_TYPE, &array_value
        ) || !mmay_mir_add_register(function, MMAY_TYPE_I64, &index)
        || !mmay_mir_add_register(
            function, MMAY_TEST_NATIVE_ACCESS_ARRAY_TYPE, &carrier
        ) || !mmay_mir_add_register(function, MMAY_TYPE_F64, &fetched)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &stored)
        || !mmay_mir_add_array(
            function, MMAY_TEST_NATIVE_ACCESS_ARRAY_TYPE,
            MMAY_TYPE_F64, &array
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
        MMAY_MIR_NO_REGISTER, 0, false, 40
    );
    instruction.f64_bits = UINT64_C(0x4014000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_ARRAY_NEW, array_value, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 41
    );
    instruction.array = array;
    instruction.array_construction = construction;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64, index, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, false, 42
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    if (kind != MMAY_TEST_NATIVE_ACCESS_DIRECT) {
        instruction = mmay_test_native_instruction(
            kind == MMAY_TEST_NATIVE_ACCESS_CHECKED_PROOF
                ? MMAY_MIR_ARRAY_REQUIRE_INDEX
                : MMAY_MIR_ARRAY_ASSUME_INDEX,
            carrier, array_value, index, 0, false, 43
        );
        instruction.array = array;
        instruction.span.end = 49;
        if (!mmay_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    } else {
        instruction = mmay_test_native_instruction(
            MMAY_MIR_ARRAY_REQUIRE_INDEX, carrier, array_value, index,
            0, false, 43
        );
        instruction.array = array;
        instruction.span.end = 49;
        if (!mmay_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    }
    instruction = mmay_test_native_instruction(
        kind == MMAY_TEST_NATIVE_ACCESS_DIRECT
            ? MMAY_MIR_ARRAY_GET : MMAY_MIR_ARRAY_GET_IN_BOUNDS,
        fetched, carrier, index, 0, false, 50
    );
    instruction.array = array;
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_native_instruction(
        kind == MMAY_TEST_NATIVE_ACCESS_DIRECT
            ? MMAY_MIR_ARRAY_SET : MMAY_MIR_ARRAY_SET_IN_BOUNDS,
        stored, carrier, index, 0, false, 51
    );
    instruction.array = array;
    instruction.third = fetched;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block, mmay_test_mir_return(stored)
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static void mmay_test_native_access_case(
    MarimayTestNativeAccessKind kind,
    unsigned char symbol_byte
) {
    MarimayMirFunction function;
    const MarimayMirFunction *functions[1];
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    MarimaySymbolId symbol;
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    memset(&symbol, symbol_byte, sizeof(symbol));
    MMAY_EXPECT(mmay_test_native_access_build(&function, symbol, kind));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(), mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_test_native_access_count(
        &generated, "mmay_managed_array_access_get("
    ) == 0);
    MMAY_EXPECT(mmay_test_native_access_count(
        &generated, "mmay_managed_array_access_set("
    ) == 0);
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_managed_array_generated_bind("
    ));
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_managed_array_generated_get_scalar("
    ));
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay_managed_array_generated_set_scalar_untracked("
    ));
    MMAY_EXPECT(!mmay_test_native_contains(
        &generated, "mmay_managed_array_view_get_raw64_fast("
    ));
    MMAY_EXPECT(!mmay_test_native_contains(
        &generated, "mmay_managed_array_view_set_raw64_fast("
    ));
    MMAY_EXPECT(!mmay_test_native_contains(
        &generated, "MarimayManagedReference *mmay_g_location_storage["
    ));
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "mmay/base/checked_i64_fast.h"
    ));
    MMAY_EXPECT(!mmay_test_native_contains(&generated, "mmay_g_array_check"));
    MMAY_EXPECT(!mmay_test_native_contains(
        &generated, "mmay_managed_array_get_in_bounds"
    ));
    MMAY_EXPECT(!mmay_test_native_contains(
        &generated, "mmay_managed_array_set_in_bounds"
    ));
    if (kind == MMAY_TEST_NATIVE_ACCESS_CHECKED_PROOF) {
        MMAY_EXPECT(mmay_test_native_access_count(
            &generated, "mmay_managed_array_access_require("
        ) == 0);
        MMAY_EXPECT(mmay_test_native_access_count(
            &generated, "mmay_g_begin_bounds("
        ) > 1);
    } else if (kind == MMAY_TEST_NATIVE_ACCESS_STATIC_PROOF) {
        MMAY_EXPECT(mmay_test_native_access_count(
            &generated, "mmay_g_begin_bounds("
        ) == 1);
    } else {
        MMAY_EXPECT(mmay_test_native_access_count(
            &generated, "mmay_g_begin_bounds("
        ) > 1);
    }
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(
        &generated, "ok:f64:4014000000000000\n", 0
    );
#endif
    mmay_owned_bytes_dispose(&generated);
    mmay_mir_function_dispose(&function);
}

void mmay_test_native_c_emitter_array_access(void) {
    mmay_test_native_access_case(MMAY_TEST_NATIVE_ACCESS_DIRECT, 0xa1);
    mmay_test_native_access_case(MMAY_TEST_NATIVE_ACCESS_CHECKED_PROOF, 0xa2);
    mmay_test_native_access_case(MMAY_TEST_NATIVE_ACCESS_STATIC_PROOF, 0xa3);
}
