#include "test_bytecode_vm_support.h"

/* A loop overwrites both a concatenation destination and a formatted string.
 * Its cumulative output is larger than its heap, while its live graph fits. */
static bool mmay_vm_string_loop(MarimayBytecodeFunction *function) {
    const MarimayBytecodeType types[] = {
        MMAY_BYTECODE_TYPE_STRING, MMAY_BYTECODE_TYPE_STRING,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_BOOL,
        MMAY_BYTECODE_TYPE_STRING
    };
    uint32_t block;
    MarimayBytecodeStringId empty;
    MarimayBytecodeStringId fragment;
    uint64_t span = 0;
    memset(function, 0, sizeof(*function));
    return mmay_bytecode_function_init(function, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_STRING, 1, NULL, 0,
            mmay_bytecode_function_default_options())
        && mmay_test_vm_add_registers(function, types, 7)
        && mmay_bytecode_add_string_constant(function, NULL, 0, &empty)
        && mmay_bytecode_add_string_constant(function,
            (const unsigned char *)"0123456789abcdef", 16, &fragment)
        && mmay_bytecode_add_block(function, &block)
        && mmay_bytecode_add_block(function, &block)
        && mmay_bytecode_add_block(function, &block)
        && mmay_bytecode_add_block(function, &block)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_CONST_STRING, 0, empty, 0, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_CONST_STRING, 1, fragment, 0, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_CONST_I64, 2, 0, 0, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_CONST_I64, 3, 128, 0, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_CONST_I64, 4, 1, 0, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_JUMP, 1, 0, 0, &span)
        && mmay_test_vm_append(function, 1, MMAY_OPCODE_I64_LESS, 5, 2, 3, &span)
        && mmay_test_vm_append(function, 1, MMAY_OPCODE_BRANCH_BOOL, 5, 2, 3, &span)
        && mmay_test_vm_append(function, 2, MMAY_OPCODE_STRING_CONCAT, 0, 0, 1, &span)
        && mmay_test_vm_append(function, 2, MMAY_OPCODE_FORMAT_INTEGER, 6, 2, 0, &span)
        && mmay_test_vm_append(function, 2, MMAY_OPCODE_I64_ADD_CHECKED, 2, 2, 4, &span)
        && mmay_test_vm_append(function, 2, MMAY_OPCODE_JUMP, 1, 0, 0, &span)
        && mmay_test_vm_append(function, 3, MMAY_OPCODE_RETURN, 0, 0, 0, &span);
}

static void mmay_vm_string_expect_report(
    MarimayBytecodeExecution *execution, MarimayBytecodeValue value
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    MMAY_EXPECT(mmay_bytecode_execution_string_bytes(execution, value, &bytes, &length));
    MMAY_EXPECT(length == 2048);
    if (length != 2048 || bytes == NULL) return;
    for (size_t index = 0; index < length; index += 16) {
        MMAY_EXPECT(memcmp(bytes + index, "0123456789abcdef", 16) == 0);
    }
}

static void mmay_vm_string_failure_sweep(
    const MarimayBytecodeFunction *function, bool force_collection
) {
    size_t attempts = 0;
    for (size_t index = 0; index <= attempts; index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeExecution execution = {0};
        MarimayBytecodeExecutionOptions setup = mmay_bytecode_execution_default_options();
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        MarimayBytecodeValue result = mmay_test_vm_i64(1234);
        MarimayBytecodeRunStatus status;
        setup.managed_heap.max_live_bytes = 8192;
        setup.managed_heap.max_objects = 16;
        setup.managed_heap.collect_before_allocation = force_collection;
        mmay_fail_allocator_init(&state, mmay_system_allocator(),
            index == 0 ? SIZE_MAX : index - 1);
        if (mmay_bytecode_execution_init(&execution, mmay_fail_allocator(&state), setup)) {
            options.execution = &execution;
            status = mmay_bytecode_run(function, NULL, 0,
                mmay_fail_allocator(&state), options, &result);
            if (index == 0) {
                attempts = state.attempts;
                MMAY_EXPECT(status == MMAY_BYTECODE_RUN_OK);
                mmay_vm_string_expect_report(&execution, result);
                MMAY_EXPECT(mmay_managed_heap_live_bytes(&execution.managed_heap) <= 8192);
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
                MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
            }
        } else {
            MMAY_EXPECT(index != 0);
        }
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(attempts > 128);
}

static void mmay_vm_string_host_lifetime(void) {
    const unsigned char text[] = {'A', 0, 0xd1, 0x8f};
    MarimayBytecodeFunction literal;
    MarimayBytecodeFunction identity;
    MarimayFailAllocator state;
    MarimayBytecodeExecution first = {0};
    MarimayBytecodeExecution second = {0};
    MarimayBytecodeExecutionOptions setup = mmay_bytecode_execution_default_options();
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue value = {0};
    MarimayBytecodeValue foreign = {0};
    MarimayBytecodeValue result = {0};
    MarimayManagedRootFrame root = {0};
    MarimayManagedReference *locations[] = {&value.as.managed_reference};
    const unsigned char *bytes = NULL;
    size_t length = 0;
    setup.managed_heap.collect_before_allocation = true;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_test_vm_build_string_literal(&literal,
        mmay_system_allocator(), text, sizeof(text), false));
    MMAY_EXPECT(mmay_test_vm_build_string_identity(&identity, mmay_system_allocator()));
    MMAY_EXPECT(mmay_bytecode_execution_init(&first, mmay_fail_allocator(&state), setup));
    MMAY_EXPECT(mmay_bytecode_execution_init(&second, mmay_fail_allocator(&state), setup));
    options.execution = &first;
    MMAY_EXPECT(mmay_bytecode_run(&literal, NULL, 0, mmay_fail_allocator(&state),
        options, &value) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(mmay_managed_root_push(&first.managed_heap, &root, locations, 1)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_bytecode_run(&literal, NULL, 0, mmay_fail_allocator(&state),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&first.managed_heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&first.managed_heap) == 1);
    MMAY_EXPECT(mmay_bytecode_run(&identity, &value, 1, mmay_fail_allocator(&state),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(mmay_bytecode_execution_string_bytes(&first, result, &bytes, &length));
    MMAY_EXPECT(length == sizeof(text) && memcmp(bytes, text, sizeof(text)) == 0);
    options.execution = &second;
    MMAY_EXPECT(mmay_bytecode_run(&literal, NULL, 0, mmay_fail_allocator(&state),
        options, &foreign) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(value.as.managed_reference.slot == foreign.as.managed_reference.slot);
    MMAY_EXPECT(!mmay_bytecode_execution_string_bytes(&second, value, &bytes, &length));
    result = mmay_test_vm_i64(4321);
    MMAY_EXPECT(mmay_bytecode_run(&identity, &value, 1, mmay_fail_allocator(&state),
        options, &result) == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 4321);
    MMAY_EXPECT(mmay_managed_root_pop(&first.managed_heap, &root) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&first.managed_heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&first.managed_heap) == 0);
    options.execution = &first;
    MMAY_EXPECT(mmay_bytecode_run(&literal, NULL, 0, mmay_fail_allocator(&state),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(value.as.managed_reference.slot == result.as.managed_reference.slot);
    MMAY_EXPECT(!mmay_bytecode_execution_string_bytes(&first, value, &bytes, &length));
    MMAY_EXPECT(mmay_bytecode_run(&identity, &value, 1, mmay_fail_allocator(&state),
        options, &result) == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
    mmay_bytecode_function_dispose(&literal);
    mmay_bytecode_function_dispose(&identity);
    mmay_bytecode_execution_dispose(&second);
    mmay_bytecode_execution_dispose(&first);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

static void mmay_vm_string_live_budget(const MarimayBytecodeFunction *function) {
    MarimayFailAllocator state;
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeExecutionOptions setup = mmay_bytecode_execution_default_options();
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayManagedResourceFailure failure;
    MarimayBytecodeValue result = mmay_test_vm_i64(9876);
    setup.managed_heap.max_live_bytes = 2048;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_fail_allocator(&state), setup));
    options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(function, NULL, 0, mmay_fail_allocator(&state),
        options, &result) == MMAY_BYTECODE_RUN_LIMIT_REACHED);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 9876);
    MMAY_EXPECT(mmay_managed_heap_last_resource_failure(&execution.managed_heap, &failure));
    MMAY_EXPECT(failure.classification == MMAY_MANAGED_RESOURCE_FAILURE_BUDGET);
    MMAY_EXPECT(failure.limit_bytes == 2048);
    MMAY_EXPECT(failure.live_bytes + failure.requested_bytes > failure.limit_bytes);
    mmay_bytecode_execution_dispose(&execution);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

void mmay_test_vm_managed_strings(void) {
    MarimayBytecodeFunction function;
    MMAY_EXPECT(mmay_vm_string_loop(&function));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_vm_string_failure_sweep(&function, false);
    mmay_vm_string_failure_sweep(&function, true);
    mmay_vm_string_live_budget(&function);
    mmay_bytecode_function_dispose(&function);
    mmay_vm_string_host_lifetime();
}
