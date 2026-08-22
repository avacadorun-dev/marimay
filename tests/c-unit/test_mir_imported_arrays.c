#include "test_imported_arrays_support.h"

#include "mmay/mir/interpret.h"

typedef struct MarimayTestMirImportedArrayCalls {
    const MarimayTestImportedArrays *test;
    MarimayAllocator allocator;
    MarimayMirExecution *execution;
} MarimayTestMirImportedArrayCalls;

static MarimayMirInterpretStatus mmay_test_mir_imported_array_call(
    void *context,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayTestMirImportedArrayCalls *calls = context;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    size_t index = 0;
    if (calls == NULL || calls->test == NULL || calls->execution == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    options.call_context = calls;
    options.call = mmay_test_mir_imported_array_call;
    options.execution = calls->execution;
    for (index = 0; index < calls->test->checked.functions.length;
        index += 1) {
        const MarimayMirFunction *function = calls->test->functions[index];
        if (function != NULL
            && mmay_symbol_id_equal(function->symbol, callee)) {
            return mmay_mir_interpret(
                function,
                arguments,
                argument_count,
                calls->allocator,
                options,
                result
            );
        }
    }
    return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
}

static bool mmay_test_mir_name(
    const MarimayOwnedBytes *bytes,
    const char *text
) {
    size_t length = strlen(text);
    return bytes != NULL && bytes->length == length
        && memcmp(bytes->data, text, length) == 0;
}

static bool mmay_test_mir_has_record_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->records.length; index += 1) {
        const MarimayMirRecord *record = mmay_vector_at_const(
            &function->records, index
        );
        if (record != NULL && record->type == type) return true;
    }
    return false;
}

static bool mmay_test_mir_has_enum_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *enum_layout = mmay_vector_at_const(
            &function->enums, index
        );
        if (enum_layout != NULL && enum_layout->type == type) return true;
    }
    return false;
}

static bool mmay_test_mir_has_array_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->arrays.length; index += 1) {
        const MarimayMirArray *array = mmay_vector_at_const(
            &function->arrays, index
        );
        if (array != NULL && array->type == type) return true;
    }
    return false;
}

static void mmay_test_mir_imported_array_closure(
    const MarimayTestImportedArrays *test
) {
    size_t function_index = 0;
    bool saw_forward_only_record_array = false;
    bool saw_nested_array = false;
    for (function_index = 0;
        function_index < test->checked.functions.length;
        function_index += 1) {
        const MarimayCheckedFunction *checked = mmay_checked_function(
            &test->checked, function_index
        );
        size_t array_index = 0;
        if (checked == NULL) continue;
        MMAY_EXPECT(mmay_mir_verify(&checked->mir) == MMAY_MIR_VERIFY_OK);
        for (array_index = 0; array_index < checked->mir.arrays.length;
            array_index += 1) {
            const MarimayMirArray *array = mmay_vector_at_const(
                &checked->mir.arrays, array_index
            );
            MarimayTypeKind element_kind = array == NULL
                ? MMAY_TYPE_KIND_INVALID
                : mmay_type_kind(
                    &test->checked.types, array->element_type
                );
            if (element_kind == MMAY_TYPE_KIND_NOMINAL) {
                bool has_layout = mmay_test_mir_has_record_type(
                        &checked->mir, array->element_type
                    ) || mmay_test_mir_has_enum_type(
                        &checked->mir, array->element_type
                    );
                MMAY_EXPECT(has_layout);
                if (mmay_test_mir_name(&checked->name, "pass")) {
                    saw_forward_only_record_array = has_layout;
                }
            } else if (element_kind == MMAY_TYPE_KIND_ARRAY) {
                MMAY_EXPECT(mmay_test_mir_has_array_type(
                    &checked->mir, array->element_type
                ));
                saw_nested_array = true;
            }
        }
    }
    MMAY_EXPECT(saw_forward_only_record_array);
    MMAY_EXPECT(saw_nested_array);
}

static void mmay_test_mir_imported_array_mutation_order(
    const MarimayTestImportedArrays *test
) {
    const MarimayCheckedFunction *mutate = mmay_checker_import_function(
        &test->checked, "replaceCell", NULL
    );
    const MarimayCheckedFunction *push = mmay_checker_import_function(
        &test->checked, "mutateCells", NULL
    );
    size_t first_require = SIZE_MAX;
    size_t first_set = SIZE_MAX;
    size_t ordinal = 0;
    size_t block_index = 0;
    MMAY_EXPECT(mutate != NULL);
    if (mutate == NULL) return;
    for (block_index = 0; block_index < mutate->mir.blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &mutate->mir.blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1, ordinal += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL) continue;
            if (instruction->kind == MMAY_MIR_ARRAY_REQUIRE_INDEX
                && first_require == SIZE_MAX) first_require = ordinal;
            if ((instruction->kind == MMAY_MIR_ARRAY_SET
                    || instruction->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS
                    || instruction->kind == MMAY_MIR_RECORD_SET)
                && first_set == SIZE_MAX) first_set = ordinal;
        }
    }
    MMAY_EXPECT(push != NULL);
    if (push != NULL) {
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &push->mir, MMAY_MIR_ARRAY_PUSH
        ) == 1);
    }
    MMAY_EXPECT(first_require != SIZE_MAX);
    MMAY_EXPECT(first_set != SIZE_MAX);
    MMAY_EXPECT(first_require < first_set);
}

static void mmay_test_mir_imported_array_allocations(
    const MarimayTestImportedArrays *test
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirExecution execution;
        MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
        MarimayTestMirImportedArrayCalls calls;
        MarimayMirValue result;
        MarimayMirInterpretStatus status = MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&calls, 0, sizeof(calls));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        if (mmay_mir_execution_init(
                &execution, allocator, mmay_mir_execution_default_options()
            )) {
            calls.test = test;
            calls.allocator = allocator;
            calls.execution = &execution;
            options.call_context = &calls;
            options.call = mmay_test_mir_imported_array_call;
            options.execution = &execution;
            status = mmay_mir_interpret(
                &test->entry->mir, NULL, 0, allocator, options, &result
            );
        }
        if (status == MMAY_MIR_INTERPRET_OK) {
            reached_success = true;
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 51);
        } else {
            MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        mmay_mir_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_mir_imported_arrays(void) {
    MarimayTestImportedArrays test;
    MMAY_EXPECT(mmay_test_imported_arrays_init(
        &test, mmay_system_allocator()
    ));
    if (!test.initialized) return;
    mmay_test_mir_imported_array_closure(&test);
    mmay_test_mir_imported_array_mutation_order(&test);
    mmay_test_mir_imported_array_allocations(&test);
    mmay_test_imported_arrays_dispose(&test);
}
