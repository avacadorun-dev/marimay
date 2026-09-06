#include "test_bytecode_support.h"

typedef enum MarimayTestEnumProvenanceShape {
    MMAY_TEST_ENUM_JOIN_MISSING,
    MMAY_TEST_ENUM_JOIN_SAME,
    MMAY_TEST_ENUM_JOIN_CONFLICT,
    MMAY_TEST_ENUM_CARRIER_OVERWRITE,
    MMAY_TEST_ENUM_SOURCE_OVERWRITE,
    MMAY_TEST_ENUM_LOOP_SAME,
    MMAY_TEST_ENUM_LOOP_CONFLICT
} MarimayTestEnumProvenanceShape;

static bool mmay_test_bytecode_provenance_init_function(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    uint32_t *checked,
    uint32_t *alternate,
    uint32_t *projected
) {
    static const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_ENUM,
        MMAY_BYTECODE_TYPE_BOOL
    };
    MarimayBytecodeEnumVariant variants[2];
    MarimayBytecodeEnumPayload payloads[2];
    MarimayBytecodeEnumId enumeration = MMAY_BYTECODE_NO_ENUM;
    MarimaySymbolId symbol = {{0x81}};
    memset(function, 0, sizeof(*function));
    memset(variants, 0, sizeof(variants));
    memset(payloads, 0, sizeof(payloads));
    variants[0].payload_start = 0;
    variants[0].payload_count = 1;
    variants[1].payload_start = 1;
    variants[1].payload_count = 1;
    payloads[0].type = MMAY_BYTECODE_TYPE_I64;
    payloads[0].layout = MMAY_BYTECODE_NO_LAYOUT;
    payloads[1].type = MMAY_BYTECODE_TYPE_I64;
    payloads[1].layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_function_init(
            function,
            allocator,
            MMAY_BYTECODE_TYPE_I64,
            1,
            parameters,
            sizeof(parameters) / sizeof(parameters[0]),
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_ENUM,
            checked
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_ENUM,
            alternate
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_I64,
            projected
        ) || !mmay_bytecode_add_enum_layout(
            function,
            symbol,
            variants,
            sizeof(variants) / sizeof(variants[0]),
            payloads,
            sizeof(payloads) / sizeof(payloads[0]),
            &enumeration
        ) || !mmay_bytecode_set_register_layout(function, 0, enumeration)
        || !mmay_bytecode_set_register_layout(
            function,
            *checked,
            enumeration
        ) || !mmay_bytecode_set_register_layout(
            function,
            *alternate,
            enumeration
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_bytecode_build_enum_join(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayTestEnumProvenanceShape shape
) {
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t blocks[4] = {
        UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
    };
    size_t span = 3000;
    MarimayOpcode second_opcode = shape == MMAY_TEST_ENUM_JOIN_MISSING
        ? MMAY_OPCODE_COPY
        : MMAY_OPCODE_ENUM_REQUIRE_VARIANT;
    uint64_t second_variant = shape == MMAY_TEST_ENUM_JOIN_CONFLICT ? 1 : 0;
    if (!mmay_test_bytecode_provenance_init_function(
            function,
            allocator,
            &checked,
            &alternate,
            &projected
        ) || !mmay_bytecode_add_block(function, &blocks[0])
        || !mmay_bytecode_add_block(function, &blocks[1])
        || !mmay_bytecode_add_block(function, &blocks[2])
        || !mmay_bytecode_add_block(function, &blocks[3])
        || !mmay_test_bytecode_append(
            function,
            blocks[0],
            MMAY_OPCODE_BRANCH_BOOL,
            1,
            blocks[1],
            blocks[2],
            &span
        ) || !mmay_test_bytecode_append(
            function,
            blocks[1],
            MMAY_OPCODE_ENUM_REQUIRE_VARIANT,
            checked,
            0,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            blocks[1],
            MMAY_OPCODE_JUMP,
            blocks[3],
            0,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            blocks[2],
            second_opcode,
            checked,
            0,
            second_variant,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            blocks[2],
            MMAY_OPCODE_JUMP,
            blocks[3],
            0,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            blocks[3],
            MMAY_OPCODE_ENUM_GET_PAYLOAD,
            projected,
            checked,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            blocks[3],
            MMAY_OPCODE_RETURN,
            projected,
            0,
            0,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (shape == MMAY_TEST_ENUM_JOIN_CONFLICT) {
        MarimayBytecodeInstruction *require = mmay_test_bytecode_at(
            function,
            blocks[2],
            0
        );
        if (require == NULL) {
            mmay_bytecode_function_dispose(function);
            return false;
        }
        require->operands[2].signed_value = 1;
    }
    (void)alternate;
    return true;
}

static bool mmay_test_bytecode_build_enum_overwrite(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    bool overwrite_carrier
) {
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 3100;
    uint32_t overwrite_target = overwrite_carrier ? checked : 0;
    uint32_t overwrite_source = overwrite_carrier ? 0 : checked;
    if (!mmay_test_bytecode_provenance_init_function(
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
    if (!mmay_bytecode_add_block(function, &block)
        || !mmay_test_bytecode_append(
            function,
            block,
            MMAY_OPCODE_ENUM_REQUIRE_VARIANT,
            checked,
            0,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            block,
            MMAY_OPCODE_COPY,
            overwrite_target,
            overwrite_source,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            block,
            MMAY_OPCODE_ENUM_GET_PAYLOAD,
            projected,
            checked,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            block,
            MMAY_OPCODE_RETURN,
            projected,
            0,
            0,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    (void)alternate;
    return true;
}

static bool mmay_test_bytecode_build_enum_loop(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    bool conflicting_backedge
) {
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t blocks[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    size_t span = 3200;
    if (!mmay_test_bytecode_provenance_init_function(
            function,
            allocator,
            &checked,
            &alternate,
            &projected
        ) || !mmay_bytecode_add_block(function, &blocks[0])
        || !mmay_bytecode_add_block(function, &blocks[1])
        || !mmay_bytecode_add_block(function, &blocks[2])
        || !mmay_test_bytecode_append(
            function,
            blocks[0],
            MMAY_OPCODE_ENUM_REQUIRE_VARIANT,
            checked,
            0,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            blocks[0],
            MMAY_OPCODE_JUMP,
            blocks[1],
            0,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            blocks[1],
            MMAY_OPCODE_ENUM_GET_PAYLOAD,
            projected,
            checked,
            0,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (conflicting_backedge && !mmay_test_bytecode_append(
            function,
            blocks[1],
            MMAY_OPCODE_ENUM_REQUIRE_VARIANT,
            checked,
            0,
            1,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (conflicting_backedge) {
        MarimayBytecodeInstruction *require = mmay_test_bytecode_at(
            function,
            blocks[1],
            1
        );
        if (require == NULL) {
            mmay_bytecode_function_dispose(function);
            return false;
        }
        require->operands[2].signed_value = 1;
    }
    if (!mmay_test_bytecode_append(
            function,
            blocks[1],
            MMAY_OPCODE_BRANCH_BOOL,
            1,
            blocks[1],
            blocks[2],
            &span
        ) || !mmay_test_bytecode_append(
            function,
            blocks[2],
            MMAY_OPCODE_RETURN,
            projected,
            0,
            0,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    (void)alternate;
    return true;
}

static void mmay_test_bytecode_enum_provenance_matrix(void) {
    static const struct {
        MarimayTestEnumProvenanceShape shape;
        MarimayBytecodeVerifyStatus expected;
    } cases[] = {
        {MMAY_TEST_ENUM_JOIN_MISSING, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT},
        {MMAY_TEST_ENUM_JOIN_SAME, MMAY_BYTECODE_VERIFY_OK},
        {MMAY_TEST_ENUM_JOIN_CONFLICT, MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT}
    };
    size_t case_index = 0;
    for (case_index = 0; case_index < sizeof(cases) / sizeof(cases[0]);
        case_index += 1) {
        MarimayBytecodeFunction function;
        MMAY_EXPECT(mmay_test_bytecode_build_enum_join(
            &function,
            mmay_system_allocator(),
            cases[case_index].shape
        ));
        MMAY_EXPECT(
            mmay_bytecode_verify(&function) == cases[case_index].expected
        );
        mmay_bytecode_function_dispose(&function);
    }
    {
        MarimayBytecodeFunction function;
        MMAY_EXPECT(mmay_test_bytecode_build_enum_overwrite(
            &function,
            mmay_system_allocator(),
            true
        ));
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
        mmay_bytecode_function_dispose(&function);
    }
    {
        MarimayBytecodeFunction function;
        MMAY_EXPECT(mmay_test_bytecode_build_enum_overwrite(
            &function,
            mmay_system_allocator(),
            false
        ));
        MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
        mmay_bytecode_function_dispose(&function);
    }
}

static void mmay_test_bytecode_enum_provenance_loop_and_work(void) {
    MarimayBytecodeFunction function;
    MMAY_EXPECT(mmay_test_bytecode_build_enum_loop(
        &function,
        mmay_system_allocator(),
        false
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    function.options.max_verify_work = 1;
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_LIMIT_REACHED);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_enum_loop(
        &function,
        mmay_system_allocator(),
        true
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
    mmay_bytecode_function_dispose(&function);
}

/* Numeric block order is deliberately opposite to control flow. A full-scan
 * fixed point revisits almost every block for each fact-propagation step. */
static bool mmay_test_bytecode_build_reverse_provenance(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    bool conflict,
    bool missing_assignment
) {
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t block = 0;
    uint32_t index = 0;
    size_t span = 3300;
    if (!mmay_test_bytecode_provenance_init_function(
            function, allocator, &checked, &alternate, &projected
        )) return false;
    for (index = 0; index < 65; index += 1) {
        if (!mmay_bytecode_add_block(function, &block)) goto fail;
    }
    if ((!missing_assignment && !mmay_test_bytecode_append(
            function, 0, MMAY_OPCODE_ENUM_REQUIRE_VARIANT, checked, 0, 0, &span
        )) || !mmay_test_bytecode_append(
            function, 0, MMAY_OPCODE_JUMP, 64, 0, 0, &span
        ) || !mmay_test_bytecode_append(
            function, 1, MMAY_OPCODE_ENUM_GET_PAYLOAD,
            projected, checked, 0, &span
        ) || !mmay_test_bytecode_append(
            function, 1, MMAY_OPCODE_RETURN, projected, 0, 0, &span
        )) goto fail;
    for (index = 2; index < 65; index += 1) {
        if (conflict && index == 32 && !mmay_test_bytecode_append(
                function, index, MMAY_OPCODE_ENUM_REQUIRE_VARIANT,
                checked, 0, 1, &span
            )) goto fail;
        if (conflict && index == 32) {
            mmay_test_bytecode_at(function, index, 0)
                ->operands[2].signed_value = 1;
        }
        if (!mmay_test_bytecode_append(
                function, index, MMAY_OPCODE_JUMP, index - 1, 0, 0, &span
            )) goto fail;
    }
    function->options.max_verify_work = 10000;
    return true;
fail:
    mmay_bytecode_function_dispose(function);
    return false;
}

static void mmay_test_bytecode_reverse_provenance(void) {
    size_t shape = 0;
    for (shape = 0; shape < 3; shape += 1) {
        MarimayBytecodeFunction function;
        MMAY_EXPECT(mmay_test_bytecode_build_reverse_provenance(
            &function, mmay_system_allocator(), shape == 1, shape == 2
        ));
        MMAY_EXPECT(mmay_bytecode_verify(&function) == (shape == 0
            ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT));
        function.options.max_verify_work = 1;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_LIMIT_REACHED);
        mmay_bytecode_function_dispose(&function);
    }
}

static void mmay_test_bytecode_enum_provenance_allocation_failures(bool reverse) {
    MarimayFailAllocator state;
    MarimayBytecodeFunction function;
    size_t function_live_allocations = 0;
    size_t function_live_bytes = 0;
    size_t failure_index = 0;
    bool reached_success = false;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(reverse
        ? mmay_test_bytecode_build_reverse_provenance(
            &function, mmay_fail_allocator(&state), false, false
        ) : mmay_test_bytecode_build_enum_loop(
            &function, mmay_fail_allocator(&state), false
        ));
    function_live_allocations = state.live_allocations;
    function_live_bytes = state.live_bytes;
    for (failure_index = 0; failure_index < 32 && !reached_success;
        failure_index += 1) {
        MarimayBytecodeVerifyStatus status;
        state.failure_index = state.attempts + failure_index;
        status = mmay_bytecode_verify(&function);
        if (status == MMAY_BYTECODE_VERIFY_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED);
        }
        MMAY_EXPECT(state.live_allocations == function_live_allocations);
        MMAY_EXPECT(state.live_bytes == function_live_bytes);
    }
    MMAY_EXPECT(reached_success);
    mmay_bytecode_function_dispose(&function);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

static void mmay_test_bytecode_array_provenance_nonwriting_barriers(void) {
    static const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_ARRAY_F64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_F64
    };
    size_t shape = 0;
    /* A fresh require after the barrier restores authority. Merely retaining
     * the same array/index registers does not. */
    for (shape = 0; shape < 4; shape += 1) {
        MarimayBytecodeFunction function;
        MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
        MarimaySymbolId callee = {{0x82}};
        uint32_t checked = UINT32_MAX;
        uint32_t value = UINT32_MAX;
        uint32_t block = UINT32_MAX;
        size_t span = 3500;
        MMAY_EXPECT(mmay_bytecode_function_init(
            &function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_F64,
            1, parameters, sizeof(parameters) / sizeof(parameters[0]),
            mmay_bytecode_function_default_options()
        ));
        MMAY_EXPECT(mmay_bytecode_add_register(
            &function, MMAY_BYTECODE_TYPE_ARRAY_F64, &checked
        ));
        MMAY_EXPECT(mmay_bytecode_add_register(
            &function, MMAY_BYTECODE_TYPE_F64, &value
        ));
        MMAY_EXPECT(mmay_bytecode_add_call(
            &function, callee, MMAY_BYTECODE_TYPE_VOID, NULL, 0, &call
        ));
        MMAY_EXPECT(mmay_bytecode_add_block(&function, &block));
        MMAY_EXPECT(mmay_test_bytecode_append(
            &function, block, MMAY_OPCODE_ARRAY_REQUIRE_INDEX,
            checked, 0, 1, &span
        ));
        MMAY_EXPECT(mmay_test_bytecode_append(
            &function, block,
            shape < 2 ? MMAY_OPCODE_CALL_DIRECT_VOID : MMAY_OPCODE_ARRAY_SET,
            shape < 2 ? call : checked, shape < 2 ? 0 : 1,
            shape < 2 ? 0 : 2, &span
        ));
        if (shape % 2 == 1) {
            MMAY_EXPECT(mmay_test_bytecode_append(
                &function, block, MMAY_OPCODE_ARRAY_REQUIRE_INDEX,
                checked, 0, 1, &span
            ));
        }
        MMAY_EXPECT(mmay_test_bytecode_append(
            &function, block, MMAY_OPCODE_ARRAY_GET_IN_BOUNDS,
            value, checked, 1, &span
        ));
        MMAY_EXPECT(mmay_test_bytecode_append(
            &function, block, MMAY_OPCODE_RETURN, value, 0, 0, &span
        ));
        MMAY_EXPECT(mmay_bytecode_verify(&function) == (shape % 2 == 1
            ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT));
        mmay_bytecode_function_dispose(&function);
    }
}

void mmay_test_bytecode_verifier_provenance(void) {
    mmay_test_bytecode_enum_provenance_matrix();
    mmay_test_bytecode_enum_provenance_loop_and_work();
    mmay_test_bytecode_reverse_provenance();
    mmay_test_bytecode_enum_provenance_allocation_failures(false);
    mmay_test_bytecode_enum_provenance_allocation_failures(true);
    mmay_test_bytecode_array_provenance_nonwriting_barriers();
}
