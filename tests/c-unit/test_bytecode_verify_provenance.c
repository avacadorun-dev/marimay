#include "test_bytecode_support.h"

typedef enum RudaTestEnumProvenanceShape {
    RUDA_TEST_ENUM_JOIN_MISSING,
    RUDA_TEST_ENUM_JOIN_SAME,
    RUDA_TEST_ENUM_JOIN_CONFLICT,
    RUDA_TEST_ENUM_CARRIER_OVERWRITE,
    RUDA_TEST_ENUM_SOURCE_OVERWRITE,
    RUDA_TEST_ENUM_LOOP_SAME,
    RUDA_TEST_ENUM_LOOP_CONFLICT
} RudaTestEnumProvenanceShape;

static bool ruda_test_bytecode_provenance_init_function(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    uint32_t *checked,
    uint32_t *alternate,
    uint32_t *projected
) {
    static const RudaBytecodeType parameters[] = {
        RUDA_BYTECODE_TYPE_ENUM,
        RUDA_BYTECODE_TYPE_BOOL
    };
    RudaBytecodeEnumVariant variants[2];
    RudaBytecodeEnumPayload payloads[2];
    RudaBytecodeEnumId enumeration = RUDA_BYTECODE_NO_ENUM;
    RudaSymbolId symbol = {{0x81}};
    memset(function, 0, sizeof(*function));
    memset(variants, 0, sizeof(variants));
    memset(payloads, 0, sizeof(payloads));
    variants[0].payload_start = 0;
    variants[0].payload_count = 1;
    variants[1].payload_start = 1;
    variants[1].payload_count = 1;
    payloads[0].type = RUDA_BYTECODE_TYPE_I64;
    payloads[0].layout = RUDA_BYTECODE_NO_LAYOUT;
    payloads[1].type = RUDA_BYTECODE_TYPE_I64;
    payloads[1].layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_function_init(
            function,
            allocator,
            RUDA_BYTECODE_TYPE_I64,
            1,
            parameters,
            sizeof(parameters) / sizeof(parameters[0]),
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_ENUM,
            checked
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_ENUM,
            alternate
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_I64,
            projected
        ) || !ruda_bytecode_add_enum_layout(
            function,
            symbol,
            variants,
            sizeof(variants) / sizeof(variants[0]),
            payloads,
            sizeof(payloads) / sizeof(payloads[0]),
            &enumeration
        ) || !ruda_bytecode_set_register_layout(function, 0, enumeration)
        || !ruda_bytecode_set_register_layout(
            function,
            *checked,
            enumeration
        ) || !ruda_bytecode_set_register_layout(
            function,
            *alternate,
            enumeration
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_bytecode_build_enum_join(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaTestEnumProvenanceShape shape
) {
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t blocks[4] = {
        UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
    };
    size_t span = 3000;
    RudaOpcode second_opcode = shape == RUDA_TEST_ENUM_JOIN_MISSING
        ? RUDA_OPCODE_COPY
        : RUDA_OPCODE_ENUM_REQUIRE_VARIANT;
    uint64_t second_variant = shape == RUDA_TEST_ENUM_JOIN_CONFLICT ? 1 : 0;
    if (!ruda_test_bytecode_provenance_init_function(
            function,
            allocator,
            &checked,
            &alternate,
            &projected
        ) || !ruda_bytecode_add_block(function, &blocks[0])
        || !ruda_bytecode_add_block(function, &blocks[1])
        || !ruda_bytecode_add_block(function, &blocks[2])
        || !ruda_bytecode_add_block(function, &blocks[3])
        || !ruda_test_bytecode_append(
            function,
            blocks[0],
            RUDA_OPCODE_BRANCH_BOOL,
            1,
            blocks[1],
            blocks[2],
            &span
        ) || !ruda_test_bytecode_append(
            function,
            blocks[1],
            RUDA_OPCODE_ENUM_REQUIRE_VARIANT,
            checked,
            0,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            blocks[1],
            RUDA_OPCODE_JUMP,
            blocks[3],
            0,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            blocks[2],
            second_opcode,
            checked,
            0,
            second_variant,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            blocks[2],
            RUDA_OPCODE_JUMP,
            blocks[3],
            0,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            blocks[3],
            RUDA_OPCODE_ENUM_GET_PAYLOAD,
            projected,
            checked,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            blocks[3],
            RUDA_OPCODE_RETURN,
            projected,
            0,
            0,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (shape == RUDA_TEST_ENUM_JOIN_CONFLICT) {
        RudaBytecodeInstruction *require = ruda_test_bytecode_at(
            function,
            blocks[2],
            0
        );
        if (require == NULL) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
        require->operands[2].signed_value = 1;
    }
    (void)alternate;
    return true;
}

static bool ruda_test_bytecode_build_enum_overwrite(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    bool overwrite_carrier
) {
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 3100;
    uint32_t overwrite_target = overwrite_carrier ? checked : 0;
    uint32_t overwrite_source = overwrite_carrier ? 0 : checked;
    if (!ruda_test_bytecode_provenance_init_function(
            function,
            allocator,
            &checked,
            &alternate,
            &projected
        )) {
        return false;
    }
    overwrite_target = overwrite_carrier ? checked : 0;
    overwrite_source = overwrite_carrier ? 0 : checked;
    if (!ruda_bytecode_add_block(function, &block)
        || !ruda_test_bytecode_append(
            function,
            block,
            RUDA_OPCODE_ENUM_REQUIRE_VARIANT,
            checked,
            0,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            block,
            RUDA_OPCODE_COPY,
            overwrite_target,
            overwrite_source,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            block,
            RUDA_OPCODE_ENUM_GET_PAYLOAD,
            projected,
            checked,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            block,
            RUDA_OPCODE_RETURN,
            projected,
            0,
            0,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    (void)alternate;
    return true;
}

static bool ruda_test_bytecode_build_enum_loop(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    bool conflicting_backedge
) {
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t blocks[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    size_t span = 3200;
    if (!ruda_test_bytecode_provenance_init_function(
            function,
            allocator,
            &checked,
            &alternate,
            &projected
        ) || !ruda_bytecode_add_block(function, &blocks[0])
        || !ruda_bytecode_add_block(function, &blocks[1])
        || !ruda_bytecode_add_block(function, &blocks[2])
        || !ruda_test_bytecode_append(
            function,
            blocks[0],
            RUDA_OPCODE_ENUM_REQUIRE_VARIANT,
            checked,
            0,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            blocks[0],
            RUDA_OPCODE_JUMP,
            blocks[1],
            0,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            blocks[1],
            RUDA_OPCODE_ENUM_GET_PAYLOAD,
            projected,
            checked,
            0,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (conflicting_backedge && !ruda_test_bytecode_append(
            function,
            blocks[1],
            RUDA_OPCODE_ENUM_REQUIRE_VARIANT,
            checked,
            0,
            1,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (conflicting_backedge) {
        RudaBytecodeInstruction *require = ruda_test_bytecode_at(
            function,
            blocks[1],
            1
        );
        if (require == NULL) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
        require->operands[2].signed_value = 1;
    }
    if (!ruda_test_bytecode_append(
            function,
            blocks[1],
            RUDA_OPCODE_BRANCH_BOOL,
            1,
            blocks[1],
            blocks[2],
            &span
        ) || !ruda_test_bytecode_append(
            function,
            blocks[2],
            RUDA_OPCODE_RETURN,
            projected,
            0,
            0,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    (void)alternate;
    return true;
}

static void ruda_test_bytecode_enum_provenance_matrix(void) {
    static const struct {
        RudaTestEnumProvenanceShape shape;
        RudaBytecodeVerifyStatus expected;
    } cases[] = {
        {RUDA_TEST_ENUM_JOIN_MISSING, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {RUDA_TEST_ENUM_JOIN_SAME, RUDA_BYTECODE_VERIFY_OK},
        {RUDA_TEST_ENUM_JOIN_CONFLICT, RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT}
    };
    size_t case_index = 0;
    for (case_index = 0; case_index < sizeof(cases) / sizeof(cases[0]);
        case_index += 1) {
        RudaBytecodeFunction function;
        RUDA_EXPECT(ruda_test_bytecode_build_enum_join(
            &function,
            ruda_system_allocator(),
            cases[case_index].shape
        ));
        RUDA_EXPECT(
            ruda_bytecode_verify(&function) == cases[case_index].expected
        );
        ruda_bytecode_function_dispose(&function);
    }
    {
        RudaBytecodeFunction function;
        RUDA_EXPECT(ruda_test_bytecode_build_enum_overwrite(
            &function,
            ruda_system_allocator(),
            true
        ));
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
        ruda_bytecode_function_dispose(&function);
    }
    {
        RudaBytecodeFunction function;
        RUDA_EXPECT(ruda_test_bytecode_build_enum_overwrite(
            &function,
            ruda_system_allocator(),
            false
        ));
        RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
        ruda_bytecode_function_dispose(&function);
    }
}

static void ruda_test_bytecode_enum_provenance_loop_and_work(void) {
    RudaBytecodeFunction function;
    RUDA_EXPECT(ruda_test_bytecode_build_enum_loop(
        &function,
        ruda_system_allocator(),
        false
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    function.options.max_verify_work = 1;
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_LIMIT_REACHED);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_enum_loop(
        &function,
        ruda_system_allocator(),
        true
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
    ruda_bytecode_function_dispose(&function);
}

static void ruda_test_bytecode_enum_provenance_allocation_failures(void) {
    RudaFailAllocator state;
    RudaBytecodeFunction function;
    size_t function_live_allocations = 0;
    size_t function_live_bytes = 0;
    size_t failure_index = 0;
    bool reached_success = false;
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    RUDA_EXPECT(ruda_test_bytecode_build_enum_loop(
        &function,
        ruda_fail_allocator(&state),
        false
    ));
    function_live_allocations = state.live_allocations;
    function_live_bytes = state.live_bytes;
    for (failure_index = 0; failure_index < 32 && !reached_success;
        failure_index += 1) {
        RudaBytecodeVerifyStatus status;
        state.failure_index = state.attempts + failure_index;
        status = ruda_bytecode_verify(&function);
        if (status == RUDA_BYTECODE_VERIFY_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED);
        }
        RUDA_EXPECT(state.live_allocations == function_live_allocations);
        RUDA_EXPECT(state.live_bytes == function_live_bytes);
    }
    RUDA_EXPECT(reached_success);
    ruda_bytecode_function_dispose(&function);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
}

void ruda_test_bytecode_verifier_provenance(void) {
    ruda_test_bytecode_enum_provenance_matrix();
    ruda_test_bytecode_enum_provenance_loop_and_work();
    ruda_test_bytecode_enum_provenance_allocation_failures();
}
