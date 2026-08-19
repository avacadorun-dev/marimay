#include "test_imported_arrays_support.h"

#include "ruda/mir/interpret.h"

typedef struct RudaTestMirImportedArrayCalls {
    const RudaTestImportedArrays *test;
    RudaAllocator allocator;
    RudaMirExecution *execution;
} RudaTestMirImportedArrayCalls;

static RudaMirInterpretStatus ruda_test_mir_imported_array_call(
    void *context,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaTestMirImportedArrayCalls *calls = context;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    size_t index = 0;
    if (calls == NULL || calls->test == NULL || calls->execution == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    options.call_context = calls;
    options.call = ruda_test_mir_imported_array_call;
    options.execution = calls->execution;
    for (index = 0; index < calls->test->checked.functions.length;
        index += 1) {
        const RudaMirFunction *function = calls->test->functions[index];
        if (function != NULL
            && ruda_symbol_id_equal(function->symbol, callee)) {
            return ruda_mir_interpret(
                function,
                arguments,
                argument_count,
                calls->allocator,
                options,
                result
            );
        }
    }
    return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
}

static bool ruda_test_mir_name(
    const RudaOwnedBytes *bytes,
    const char *text
) {
    size_t length = strlen(text);
    return bytes != NULL && bytes->length == length
        && memcmp(bytes->data, text, length) == 0;
}

static bool ruda_test_mir_has_record_type(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->records.length; index += 1) {
        const RudaMirRecord *record = ruda_vector_at_const(
            &function->records, index
        );
        if (record != NULL && record->type == type) return true;
    }
    return false;
}

static bool ruda_test_mir_has_enum_type(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->enums.length; index += 1) {
        const RudaMirEnum *enum_layout = ruda_vector_at_const(
            &function->enums, index
        );
        if (enum_layout != NULL && enum_layout->type == type) return true;
    }
    return false;
}

static bool ruda_test_mir_has_array_type(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->arrays.length; index += 1) {
        const RudaMirArray *array = ruda_vector_at_const(
            &function->arrays, index
        );
        if (array != NULL && array->type == type) return true;
    }
    return false;
}

static void ruda_test_mir_imported_array_closure(
    const RudaTestImportedArrays *test
) {
    size_t function_index = 0;
    bool saw_forward_only_record_array = false;
    bool saw_nested_array = false;
    for (function_index = 0;
        function_index < test->checked.functions.length;
        function_index += 1) {
        const RudaCheckedFunction *checked = ruda_checked_function(
            &test->checked, function_index
        );
        size_t array_index = 0;
        if (checked == NULL) continue;
        RUDA_EXPECT(ruda_mir_verify(&checked->mir) == RUDA_MIR_VERIFY_OK);
        for (array_index = 0; array_index < checked->mir.arrays.length;
            array_index += 1) {
            const RudaMirArray *array = ruda_vector_at_const(
                &checked->mir.arrays, array_index
            );
            RudaTypeKind element_kind = array == NULL
                ? RUDA_TYPE_KIND_INVALID
                : ruda_type_kind(
                    &test->checked.types, array->element_type
                );
            if (element_kind == RUDA_TYPE_KIND_NOMINAL) {
                bool has_layout = ruda_test_mir_has_record_type(
                        &checked->mir, array->element_type
                    ) || ruda_test_mir_has_enum_type(
                        &checked->mir, array->element_type
                    );
                RUDA_EXPECT(has_layout);
                if (ruda_test_mir_name(&checked->name, "pass")) {
                    saw_forward_only_record_array = has_layout;
                }
            } else if (element_kind == RUDA_TYPE_KIND_ARRAY) {
                RUDA_EXPECT(ruda_test_mir_has_array_type(
                    &checked->mir, array->element_type
                ));
                saw_nested_array = true;
            }
        }
    }
    RUDA_EXPECT(saw_forward_only_record_array);
    RUDA_EXPECT(saw_nested_array);
}

static void ruda_test_mir_imported_array_mutation_order(
    const RudaTestImportedArrays *test
) {
    const RudaCheckedFunction *mutate = ruda_checker_import_function(
        &test->checked, "replaceCell", NULL
    );
    const RudaCheckedFunction *push = ruda_checker_import_function(
        &test->checked, "mutateCells", NULL
    );
    size_t first_require = SIZE_MAX;
    size_t first_set = SIZE_MAX;
    size_t ordinal = 0;
    size_t block_index = 0;
    RUDA_EXPECT(mutate != NULL);
    if (mutate == NULL) return;
    for (block_index = 0; block_index < mutate->mir.blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &mutate->mir.blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1, ordinal += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL) continue;
            if (instruction->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX
                && first_require == SIZE_MAX) first_require = ordinal;
            if ((instruction->kind == RUDA_MIR_ARRAY_SET
                    || instruction->kind == RUDA_MIR_RECORD_SET)
                && first_set == SIZE_MAX) first_set = ordinal;
        }
    }
    RUDA_EXPECT(push != NULL);
    if (push != NULL) {
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &push->mir, RUDA_MIR_ARRAY_PUSH
        ) == 1);
    }
    RUDA_EXPECT(first_require != SIZE_MAX);
    RUDA_EXPECT(first_set != SIZE_MAX);
    RUDA_EXPECT(first_require < first_set);
}

static void ruda_test_mir_imported_array_allocations(
    const RudaTestImportedArrays *test
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaMirExecution execution;
        RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
        RudaTestMirImportedArrayCalls calls;
        RudaMirValue result;
        RudaMirInterpretStatus status = RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&calls, 0, sizeof(calls));
        memset(&result, 0, sizeof(result));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        allocator = ruda_fail_allocator(&state);
        if (ruda_mir_execution_init(
                &execution, allocator, ruda_mir_execution_default_options()
            )) {
            calls.test = test;
            calls.allocator = allocator;
            calls.execution = &execution;
            options.call_context = &calls;
            options.call = ruda_test_mir_imported_array_call;
            options.execution = &execution;
            status = ruda_mir_interpret(
                &test->entry->mir, NULL, 0, allocator, options, &result
            );
        }
        if (status == RUDA_MIR_INTERPRET_OK) {
            reached_success = true;
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 51);
        } else {
            RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        ruda_mir_execution_dispose(&execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_mir_imported_arrays(void) {
    RudaTestImportedArrays test;
    RUDA_EXPECT(ruda_test_imported_arrays_init(
        &test, ruda_system_allocator()
    ));
    if (!test.initialized) return;
    ruda_test_mir_imported_array_closure(&test);
    ruda_test_mir_imported_array_mutation_order(&test);
    ruda_test_mir_imported_array_allocations(&test);
    ruda_test_imported_arrays_dispose(&test);
}
