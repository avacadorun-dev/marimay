#include "test_mir_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

#include "ruda/mir/range.h"

enum {
    RUDA_TEST_NATIVE_ACCESS_ARRAY_TYPE = RUDA_TYPE_FIRST_DYNAMIC + 151
};

typedef enum RudaTestNativeAccessKind {
    RUDA_TEST_NATIVE_ACCESS_DIRECT,
    RUDA_TEST_NATIVE_ACCESS_CHECKED_PROOF,
    RUDA_TEST_NATIVE_ACCESS_STATIC_PROOF
} RudaTestNativeAccessKind;

static size_t ruda_test_native_access_count(
    const RudaOwnedBytes *bytes,
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

static bool ruda_test_native_access_build(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaTestNativeAccessKind kind
) {
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister array_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister carrier = RUDA_MIR_NO_REGISTER;
    RudaMirRegister fetched = RUDA_MIR_NO_REGISTER;
    RudaMirRegister stored = RUDA_MIR_NO_REGISTER;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId construction = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirArrayArgument argument;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol, NULL, 0,
            RUDA_TYPE_F64, ruda_mir_function_default_options()
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &value)
        || !ruda_mir_add_register(
            function, RUDA_TEST_NATIVE_ACCESS_ARRAY_TYPE, &array_value
        ) || !ruda_mir_add_register(function, RUDA_TYPE_I64, &index)
        || !ruda_mir_add_register(
            function, RUDA_TEST_NATIVE_ACCESS_ARRAY_TYPE, &carrier
        ) || !ruda_mir_add_register(function, RUDA_TYPE_F64, &fetched)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &stored)
        || !ruda_mir_add_array(
            function, RUDA_TEST_NATIVE_ACCESS_ARRAY_TYPE,
            RUDA_TYPE_F64, &array
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
        RUDA_MIR_NO_REGISTER, 0, false, 40
    );
    instruction.f64_bits = UINT64_C(0x4014000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_ARRAY_NEW, array_value, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 41
    );
    instruction.array = array;
    instruction.array_construction = construction;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64, index, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, false, 42
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    if (kind != RUDA_TEST_NATIVE_ACCESS_DIRECT) {
        instruction = ruda_test_native_instruction(
            kind == RUDA_TEST_NATIVE_ACCESS_CHECKED_PROOF
                ? RUDA_MIR_ARRAY_REQUIRE_INDEX
                : RUDA_MIR_ARRAY_ASSUME_INDEX,
            carrier, array_value, index, 0, false, 43
        );
        instruction.array = array;
        instruction.span.end = 49;
        if (!ruda_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    } else {
        instruction = ruda_test_native_instruction(
            RUDA_MIR_ARRAY_REQUIRE_INDEX, carrier, array_value, index,
            0, false, 43
        );
        instruction.array = array;
        instruction.span.end = 49;
        if (!ruda_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    }
    instruction = ruda_test_native_instruction(
        kind == RUDA_TEST_NATIVE_ACCESS_DIRECT
            ? RUDA_MIR_ARRAY_GET : RUDA_MIR_ARRAY_GET_IN_BOUNDS,
        fetched, carrier, index, 0, false, 50
    );
    instruction.array = array;
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_native_instruction(
        kind == RUDA_TEST_NATIVE_ACCESS_DIRECT
            ? RUDA_MIR_ARRAY_SET : RUDA_MIR_ARRAY_SET_IN_BOUNDS,
        stored, carrier, index, 0, false, 51
    );
    instruction.array = array;
    instruction.third = fetched;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block, ruda_test_mir_return(stored)
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static void ruda_test_native_access_case(
    RudaTestNativeAccessKind kind,
    unsigned char symbol_byte
) {
    RudaMirFunction function;
    const RudaMirFunction *functions[1];
    RudaNativeCProgram program;
    RudaOwnedBytes generated;
    RudaSymbolId symbol;
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    memset(&symbol, symbol_byte, sizeof(symbol));
    RUDA_EXPECT(ruda_test_native_access_build(&function, symbol, kind));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    functions[0] = &function;
    program.functions = functions;
    program.function_count = 1;
    program.entry_symbol = symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &program, ruda_system_allocator(), ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(ruda_test_native_access_count(
        &generated, "ruda_managed_array_access_get("
    ) == 0);
    RUDA_EXPECT(ruda_test_native_access_count(
        &generated, "ruda_managed_array_access_set("
    ) == 0);
    RUDA_EXPECT(ruda_test_native_contains(
        &generated, "ruda_managed_array_view_bind("
    ));
    RUDA_EXPECT(ruda_test_native_contains(
        &generated, "memcpy(&ruda_g_bits, *ruda_g_av_"
    ));
    RUDA_EXPECT(ruda_test_native_contains(
        &generated, "memcpy(*ruda_g_av_"
    ));
    RUDA_EXPECT(!ruda_test_native_contains(
        &generated, "ruda_managed_array_view_get_raw64_fast("
    ));
    RUDA_EXPECT(!ruda_test_native_contains(
        &generated, "ruda_managed_array_view_set_raw64_fast("
    ));
    RUDA_EXPECT(ruda_test_native_contains(
        &generated, "RudaManagedReference *ruda_g_location_storage["
    ));
    RUDA_EXPECT(ruda_test_native_contains(
        &generated, "ruda/base/checked_i64_fast.h"
    ));
    RUDA_EXPECT(!ruda_test_native_contains(&generated, "ruda_g_array_check"));
    RUDA_EXPECT(!ruda_test_native_contains(
        &generated, "ruda_managed_array_get_in_bounds"
    ));
    RUDA_EXPECT(!ruda_test_native_contains(
        &generated, "ruda_managed_array_set_in_bounds"
    ));
    if (kind == RUDA_TEST_NATIVE_ACCESS_CHECKED_PROOF) {
        RUDA_EXPECT(ruda_test_native_access_count(
            &generated, "ruda_managed_array_access_require("
        ) == 0);
        RUDA_EXPECT(ruda_test_native_access_count(
            &generated, "ruda_g_begin_bounds("
        ) > 1);
    } else if (kind == RUDA_TEST_NATIVE_ACCESS_STATIC_PROOF) {
        RUDA_EXPECT(ruda_test_native_access_count(
            &generated, "ruda_g_begin_bounds("
        ) == 1);
    } else {
        RUDA_EXPECT(ruda_test_native_access_count(
            &generated, "ruda_g_begin_bounds("
        ) > 1);
    }
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(
        &generated, "ok:f64:4014000000000000\n", 0
    );
#endif
    ruda_owned_bytes_dispose(&generated);
    ruda_mir_function_dispose(&function);
}

void ruda_test_native_c_emitter_array_access(void) {
    ruda_test_native_access_case(RUDA_TEST_NATIVE_ACCESS_DIRECT, 0xa1);
    ruda_test_native_access_case(RUDA_TEST_NATIVE_ACCESS_CHECKED_PROOF, 0xa2);
    ruda_test_native_access_case(RUDA_TEST_NATIVE_ACCESS_STATIC_PROOF, 0xa3);
}
