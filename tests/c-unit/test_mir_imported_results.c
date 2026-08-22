#include "test_imported_results_support.h"
#include "test_mir_support.h"

#include "mmay/mir/interpret.h"

typedef struct MarimayTestMirImportedResultCalls {
    const MarimayTestImportedResults *test;
    MarimayAllocator allocator;
    MarimayMirExecution *execution;
} MarimayTestMirImportedResultCalls;

static MarimayMirInterpretStatus mmay_test_mir_imported_result_call(
    void *context,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayTestMirImportedResultCalls *calls = context;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    size_t index = 0;
    if (calls == NULL || calls->test == NULL || calls->execution == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    options.call_context = calls;
    options.call = mmay_test_mir_imported_result_call;
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

static const MarimayCheckedEnum *mmay_test_mir_imported_choice(
    const MarimayTestImportedResults *test
) {
    size_t index = 0;
    for (index = 0; index < test->checked.enums.length; index += 1) {
        const MarimayCheckedEnum *enum_value = mmay_checked_enum(
            &test->checked, index
        );
        if (enum_value != NULL && enum_value->name.length == 6
            && memcmp(enum_value->name.data, "Choice", 6) == 0) {
            return enum_value;
        }
    }
    return NULL;
}

static void mmay_test_mir_imported_result_identity(
    const MarimayTestImportedResults *test
) {
    const MarimayCheckedEnum *choice = mmay_test_mir_imported_choice(test);
    MarimayTypeId choice_array_type = MMAY_TYPE_INVALID;
    bool saw_producer_construction = false;
    bool saw_consumer_construction = false;
    bool saw_consumer_match = false;
    size_t function_index = 0;
    MMAY_EXPECT(choice != NULL);
    for (function_index = 0;
        function_index < test->checked.functions.length;
        function_index += 1) {
        const MarimayCheckedFunction *checked = mmay_checked_function(
            &test->checked, function_index
        );
        size_t enum_index = 0;
        if (checked == NULL) {
            continue;
        }
        MMAY_EXPECT(mmay_mir_verify(&checked->mir) == MMAY_MIR_VERIFY_OK);
        for (enum_index = 0; enum_index < checked->mir.enums.length;
            enum_index += 1) {
            const MarimayMirEnum *layout = mmay_vector_at_const(
                &checked->mir.enums, enum_index
            );
            const MarimayTypeDescriptor *descriptor = layout == NULL
                ? NULL : mmay_type_descriptor(
                    &test->checked.types, layout->type
                );
            if (choice == NULL || descriptor == NULL
                || !mmay_symbol_id_equal(
                    descriptor->symbol, choice->symbol
                ) || descriptor->argument_count != 1) {
                continue;
            }
            MMAY_EXPECT(mmay_type_kind(
                &test->checked.types, descriptor->arguments[0]
            ) == MMAY_TYPE_KIND_ARRAY);
            if (choice_array_type == MMAY_TYPE_INVALID) {
                choice_array_type = layout->type;
            } else {
                MMAY_EXPECT(choice_array_type == layout->type);
            }
            MMAY_EXPECT(layout->variant_count == 2);
            MMAY_EXPECT(layout->payload_count == 1);
            if (checked->name.length == 6
                && memcmp(checked->name.data, "choice", 6) == 0) {
                saw_producer_construction =
                    mmay_test_mir_instruction_kind_count(
                        &checked->mir, MMAY_MIR_ENUM_NEW
                    ) != 0;
            }
            if ((checked->name.length == 14
                    && memcmp(
                        checked->name.data, "namespaceScore", 14
                    ) == 0)
                || (checked->name.length == 10
                    && memcmp(
                        checked->name.data, "relayScore", 10
                    ) == 0)) {
                saw_consumer_construction =
                    saw_consumer_construction
                    || mmay_test_mir_instruction_kind_count(
                        &checked->mir, MMAY_MIR_ENUM_NEW
                    ) != 0;
                saw_consumer_match = saw_consumer_match
                    || mmay_test_mir_instruction_kind_count(
                        &checked->mir, MMAY_MIR_ENUM_IS_VARIANT
                    ) != 0;
            }
        }
    }
    MMAY_EXPECT(choice_array_type != MMAY_TYPE_INVALID);
    MMAY_EXPECT(saw_producer_construction);
    MMAY_EXPECT(saw_consumer_construction);
    MMAY_EXPECT(saw_consumer_match);
}

static MarimayMirInterpretStatus mmay_test_mir_imported_result_execute(
    const MarimayTestImportedResults *test,
    MarimayAllocator allocator,
    MarimayMirValue *result
) {
    MarimayMirExecution execution;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayTestMirImportedResultCalls calls;
    MarimayMirInterpretStatus status = MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    memset(&execution, 0, sizeof(execution));
    memset(&calls, 0, sizeof(calls));
    if (!mmay_mir_execution_init(
            &execution, allocator, mmay_mir_execution_default_options()
        )) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    calls.test = test;
    calls.allocator = allocator;
    calls.execution = &execution;
    options.call_context = &calls;
    options.call = mmay_test_mir_imported_result_call;
    options.execution = &execution;
    status = mmay_mir_interpret(
        &test->entry->mir, NULL, 0, allocator, options, result
    );
    mmay_mir_execution_dispose(&execution);
    return status;
}

static void mmay_test_mir_imported_result_allocations(
    const MarimayTestImportedResults *test
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayMirValue result;
        MarimayMirInterpretStatus status;
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_test_mir_imported_result_execute(
            test, mmay_fail_allocator(&state), &result
        );
        if (status == MMAY_MIR_INTERPRET_OK) {
            reached_success = true;
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 119);
        } else {
            MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_mir_imported_results(void) {
    MarimayTestImportedResults test;
    MarimayMirValue result;
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_test_imported_results_init(
        &test, mmay_system_allocator()
    ));
    if (!test.initialized) {
        return;
    }
    mmay_test_mir_imported_result_identity(&test);
    MMAY_EXPECT(mmay_test_mir_imported_result_execute(
        &test, mmay_system_allocator(), &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64);
    MMAY_EXPECT(result.as.i64 == 119);
    mmay_test_mir_imported_result_allocations(&test);
    mmay_test_imported_results_dispose(&test);
}
