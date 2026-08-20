#include "test_imported_results_support.h"
#include "test_mir_support.h"

#include "ruda/mir/interpret.h"

typedef struct RudaTestMirImportedResultCalls {
    const RudaTestImportedResults *test;
    RudaAllocator allocator;
    RudaMirExecution *execution;
} RudaTestMirImportedResultCalls;

static RudaMirInterpretStatus ruda_test_mir_imported_result_call(
    void *context,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaTestMirImportedResultCalls *calls = context;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    size_t index = 0;
    if (calls == NULL || calls->test == NULL || calls->execution == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    options.call_context = calls;
    options.call = ruda_test_mir_imported_result_call;
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

static const RudaCheckedEnum *ruda_test_mir_imported_choice(
    const RudaTestImportedResults *test
) {
    size_t index = 0;
    for (index = 0; index < test->checked.enums.length; index += 1) {
        const RudaCheckedEnum *enum_value = ruda_checked_enum(
            &test->checked, index
        );
        if (enum_value != NULL && enum_value->name.length == 6
            && memcmp(enum_value->name.data, "Choice", 6) == 0) {
            return enum_value;
        }
    }
    return NULL;
}

static void ruda_test_mir_imported_result_identity(
    const RudaTestImportedResults *test
) {
    const RudaCheckedEnum *choice = ruda_test_mir_imported_choice(test);
    RudaTypeId choice_array_type = RUDA_TYPE_INVALID;
    bool saw_producer_construction = false;
    bool saw_consumer_construction = false;
    bool saw_consumer_match = false;
    size_t function_index = 0;
    RUDA_EXPECT(choice != NULL);
    for (function_index = 0;
        function_index < test->checked.functions.length;
        function_index += 1) {
        const RudaCheckedFunction *checked = ruda_checked_function(
            &test->checked, function_index
        );
        size_t enum_index = 0;
        if (checked == NULL) {
            continue;
        }
        RUDA_EXPECT(ruda_mir_verify(&checked->mir) == RUDA_MIR_VERIFY_OK);
        for (enum_index = 0; enum_index < checked->mir.enums.length;
            enum_index += 1) {
            const RudaMirEnum *layout = ruda_vector_at_const(
                &checked->mir.enums, enum_index
            );
            const RudaTypeDescriptor *descriptor = layout == NULL
                ? NULL : ruda_type_descriptor(
                    &test->checked.types, layout->type
                );
            if (choice == NULL || descriptor == NULL
                || !ruda_symbol_id_equal(
                    descriptor->symbol, choice->symbol
                ) || descriptor->argument_count != 1) {
                continue;
            }
            RUDA_EXPECT(ruda_type_kind(
                &test->checked.types, descriptor->arguments[0]
            ) == RUDA_TYPE_KIND_ARRAY);
            if (choice_array_type == RUDA_TYPE_INVALID) {
                choice_array_type = layout->type;
            } else {
                RUDA_EXPECT(choice_array_type == layout->type);
            }
            RUDA_EXPECT(layout->variant_count == 2);
            RUDA_EXPECT(layout->payload_count == 1);
            if (checked->name.length == 6
                && memcmp(checked->name.data, "choice", 6) == 0) {
                saw_producer_construction =
                    ruda_test_mir_instruction_kind_count(
                        &checked->mir, RUDA_MIR_ENUM_NEW
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
                    || ruda_test_mir_instruction_kind_count(
                        &checked->mir, RUDA_MIR_ENUM_NEW
                    ) != 0;
                saw_consumer_match = saw_consumer_match
                    || ruda_test_mir_instruction_kind_count(
                        &checked->mir, RUDA_MIR_ENUM_IS_VARIANT
                    ) != 0;
            }
        }
    }
    RUDA_EXPECT(choice_array_type != RUDA_TYPE_INVALID);
    RUDA_EXPECT(saw_producer_construction);
    RUDA_EXPECT(saw_consumer_construction);
    RUDA_EXPECT(saw_consumer_match);
}

static RudaMirInterpretStatus ruda_test_mir_imported_result_execute(
    const RudaTestImportedResults *test,
    RudaAllocator allocator,
    RudaMirValue *result
) {
    RudaMirExecution execution;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    RudaTestMirImportedResultCalls calls;
    RudaMirInterpretStatus status = RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    memset(&execution, 0, sizeof(execution));
    memset(&calls, 0, sizeof(calls));
    if (!ruda_mir_execution_init(
            &execution, allocator, ruda_mir_execution_default_options()
        )) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    calls.test = test;
    calls.allocator = allocator;
    calls.execution = &execution;
    options.call_context = &calls;
    options.call = ruda_test_mir_imported_result_call;
    options.execution = &execution;
    status = ruda_mir_interpret(
        &test->entry->mir, NULL, 0, allocator, options, result
    );
    ruda_mir_execution_dispose(&execution);
    return status;
}

static void ruda_test_mir_imported_result_allocations(
    const RudaTestImportedResults *test
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaMirValue result;
        RudaMirInterpretStatus status;
        memset(&result, 0, sizeof(result));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_test_mir_imported_result_execute(
            test, ruda_fail_allocator(&state), &result
        );
        if (status == RUDA_MIR_INTERPRET_OK) {
            reached_success = true;
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 119);
        } else {
            RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_mir_imported_results(void) {
    RudaTestImportedResults test;
    RudaMirValue result;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_test_imported_results_init(
        &test, ruda_system_allocator()
    ));
    if (!test.initialized) {
        return;
    }
    ruda_test_mir_imported_result_identity(&test);
    RUDA_EXPECT(ruda_test_mir_imported_result_execute(
        &test, ruda_system_allocator(), &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_I64);
    RUDA_EXPECT(result.as.i64 == 119);
    ruda_test_mir_imported_result_allocations(&test);
    ruda_test_imported_results_dispose(&test);
}
