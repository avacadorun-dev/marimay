#include "test_bytecode_vm_support.h"
#include "../../bootstrap/src/bytecode/vm_image.h"

static bool mmay_image_follow_function(
    MarimayBytecodeFunction *function, bool inverted
) {
    const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_RECORD, MMAY_BYTECODE_TYPE_I64
    };
    const MarimayBytecodeType locals[] = {
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_ARRAY,
        MMAY_BYTECODE_TYPE_ARRAY, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_ARRAY,
        MMAY_BYTECODE_TYPE_ARRAY, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_BOOL
    };
    MarimayBytecodeRecordField field = {
        MMAY_BYTECODE_TYPE_ARRAY, 0, true
    };
    MarimaySymbolId layout_symbol = {{0x31}};
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    uint32_t array = UINT32_MAX;
    uint32_t blocks[4] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    uint64_t span = 100;
    memset(function, 0, sizeof(*function));
    bool ok = mmay_bytecode_function_init(function, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64, 1, parameters, 2,
            mmay_bytecode_function_default_options())
        && mmay_bytecode_add_array_type(function, MMAY_BYTECODE_TYPE_I64,
            MMAY_BYTECODE_NO_LAYOUT, &array) && array == 0
        && mmay_bytecode_add_record_layout(
            function, layout_symbol, &field, 1, &record) && record == 0
        && mmay_test_vm_add_registers(
            function, locals, sizeof(locals) / sizeof(locals[0]));
    for (size_t index = 0; ok && index < 4; index += 1) {
        ok = mmay_bytecode_add_block(function, &blocks[index])
            && blocks[index] == index;
    }
    ok = ok && mmay_bytecode_set_register_layout(function, 0, 0)
        && mmay_bytecode_set_register_layout(function, 3, 0)
        && mmay_bytecode_set_register_layout(function, 4, 0)
        && mmay_bytecode_set_register_layout(function, 7, 0)
        && mmay_bytecode_set_register_layout(function, 8, 0)
        && mmay_test_vm_append(
            function, 0, MMAY_OPCODE_COPY, 2, 1, 0, &span)
        && mmay_test_vm_append(
            function, 0, MMAY_OPCODE_JUMP, 1, 0, 0, &span)
        && mmay_test_vm_append(
            function, 1, MMAY_OPCODE_RECORD_GET, 3, 0, 0, &span)
        && mmay_test_vm_append(
            function, 1, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 4, 3, 2, &span)
        && mmay_test_vm_append(
            function, 1, MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, 5, 4, 2, &span)
        && mmay_test_vm_append(
            function, 1, MMAY_OPCODE_I64_EQUAL, 6, 5, 2, &span)
        && (!inverted || mmay_test_vm_append(
            function, 1, MMAY_OPCODE_BOOL_NOT, 10, 6, 0, &span))
        && mmay_test_vm_append(function, 1, MMAY_OPCODE_BRANCH_BOOL,
            inverted ? 10 : 6, inverted ? 2 : 3, inverted ? 3 : 2, &span)
        && mmay_test_vm_append(
            function, 2, MMAY_OPCODE_RECORD_GET, 7, 0, 0, &span)
        && mmay_test_vm_append(
            function, 2, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 8, 7, 2, &span)
        && mmay_test_vm_append(
            function, 2, MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, 9, 8, 2, &span)
        && mmay_test_vm_append(
            function, 2, MMAY_OPCODE_COPY, 2, 9, 0, &span)
        && mmay_test_vm_append(
            function, 2, MMAY_OPCODE_JUMP, 1, 0, 0, &span)
        && mmay_test_vm_append(
            function, 3, MMAY_OPCODE_RETURN, 2, 0, 0, &span);
    if (!ok || mmay_bytecode_verify(function) != MMAY_BYTECODE_VERIFY_OK) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_image_follow_program(
    MarimayBytecodeProgram *program, bool inverted
) {
    MarimayBytecodeFunction function = {0};
    MarimaySymbolId symbol = {{0x40}};
    uint32_t id = UINT32_MAX;
    memset(program, 0, sizeof(*program));
    bool ok = mmay_image_follow_function(&function, inverted)
        && mmay_bytecode_program_init(program, mmay_system_allocator(), 1,
            mmay_bytecode_program_default_options())
        && mmay_bytecode_program_take_function(
            program, symbol, &function, &id) && id == 0
        && mmay_bytecode_program_set_entry(program, symbol)
        && mmay_bytecode_program_verify(program)
            == MMAY_BYTECODE_PROGRAM_VERIFY_OK;
    mmay_bytecode_function_dispose(&function);
    if (!ok) mmay_bytecode_program_dispose(program);
    return ok;
}

static bool mmay_image_record_factory(MarimayBytecodeFunction *function) {
    const MarimayBytecodeType locals[] = {
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_ARRAY,
        MMAY_BYTECODE_TYPE_RECORD
    };
    MarimayBytecodeRecordField field = {
        MMAY_BYTECODE_TYPE_ARRAY, 0, true
    };
    MarimaySymbolId layout_symbol = {{0x31}};
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    uint32_t array = UINT32_MAX, block = UINT32_MAX;
    MarimayBytecodeArrayConstructionId array_value =
        MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
    MarimayBytecodeRecordConstructionId record_value =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeArrayArgument elements[] = {
        {0, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT},
        {0, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT}
    };
    MarimayBytecodeRecordArgument record_field = {
        1, MMAY_BYTECODE_TYPE_ARRAY, 0
    };
    uint64_t span = 200;
    memset(function, 0, sizeof(*function));
    bool ok = mmay_bytecode_function_init(function, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_RECORD, 1, NULL, 0,
            mmay_bytecode_function_default_options())
        && mmay_bytecode_add_array_type(function, MMAY_BYTECODE_TYPE_I64,
            MMAY_BYTECODE_NO_LAYOUT, &array) && array == 0
        && mmay_bytecode_add_record_layout(
            function, layout_symbol, &field, 1, &record) && record == 0
        && mmay_test_vm_add_registers(
            function, locals, sizeof(locals) / sizeof(locals[0]))
        && mmay_bytecode_set_register_layout(function, 1, 0)
        && mmay_bytecode_set_register_layout(function, 2, 0)
        && mmay_bytecode_add_array_construction_with_layout(function,
            MMAY_BYTECODE_TYPE_ARRAY, 0, elements, 2, &array_value)
        && mmay_bytecode_add_record_construction(
            function, 0, &record_field, 1, &record_value)
        && mmay_bytecode_add_block(function, &block)
        && mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64, 0, 1, 0, &span)
        && mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_NEW, 1, array_value, 0, &span)
        && mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_NEW, 2, record_value, 0, &span)
        && mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, 2, 0, 0, &span);
    function->result_layout = 0;
    if (!ok || mmay_bytecode_verify(function) != MMAY_BYTECODE_VERIFY_OK) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

typedef struct MarimayImageObservation {
    MarimayBytecodeRunStatus status;
    MarimayBytecodeValue result;
    size_t remaining;
    int64_t parent[2];
} MarimayImageObservation;

static MarimayImageObservation mmay_image_observe_follow(
    const MarimayBytecodeProgram *program, bool image, bool shared,
    size_t fuel
) {
    MarimayFailAllocator state;
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeFunction factory = {0};
    MarimayBytecodeValue arguments[2] = {{0}};
    MarimayImageObservation observed = {0};
    MarimayManagedRootFrame roots = {0};
    MarimayManagedReference *locations[] = {
        &arguments[0].as.managed_reference
    };
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, allocator,
        mmay_bytecode_execution_default_options()));
    MMAY_EXPECT(mmay_image_record_factory(&factory));
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(&factory, NULL, 0, allocator, options,
        &arguments[0]) == MMAY_BYTECODE_RUN_OK);
    arguments[1] = mmay_test_vm_i64(0);
    MMAY_EXPECT(mmay_managed_root_push(&execution.managed_heap, &roots,
        locations, 1) == MMAY_MANAGED_OK);
    observed.result = mmay_test_vm_i64(1234);
    observed.remaining = fuel;
    options.max_steps = shared ? SIZE_MAX : fuel;
    options.remaining_shared_steps = shared ? &observed.remaining : NULL;
    observed.status = image
        ? mmay_bytecode_run_program(program, arguments, 2, allocator,
            options, &observed.result)
        : mmay_bytecode_run(&mmay_bytecode_program_function(program, 0)->body,
            arguments, 2, allocator, options, &observed.result);
    MarimayManagedCell array = {0}, value = {0};
    size_t length = 0;
    MMAY_EXPECT(mmay_managed_record_get(&execution.managed_heap,
        arguments[0].as.managed_reference, 0, &array) == MMAY_MANAGED_OK);
    for (size_t index = 0; index < 2; index += 1) {
        MMAY_EXPECT(mmay_managed_array_get(&execution.managed_heap,
            array.reference, (int64_t)index, &value, &length)
            == MMAY_MANAGED_OK);
        observed.parent[index] = (int64_t)value.f64_bits;
    }
    MMAY_EXPECT(mmay_managed_root_pop(
        &execution.managed_heap, &roots) == MMAY_MANAGED_OK);
    mmay_bytecode_function_dispose(&factory);
    mmay_bytecode_execution_dispose(&execution);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    return observed;
}

static void mmay_image_fuel_contract(
    const MarimayBytecodeProgram *program
) {
    for (size_t fuel = 1; fuel <= 19; fuel += 1) {
        for (unsigned shared = 0; shared < 2; shared += 1) {
            MarimayImageObservation cold = mmay_image_observe_follow(
                program, false, shared != 0, fuel);
            MarimayImageObservation image = mmay_image_observe_follow(
                program, true, shared != 0, fuel);
            MMAY_EXPECT(cold.status == image.status
                && cold.result.type == image.result.type
                && cold.result.as.i64 == image.result.as.i64
                && cold.remaining == image.remaining
                && cold.parent[0] == image.parent[0]
                && cold.parent[1] == image.parent[1]);
        }
    }
}

static void mmay_image_direct_bindings(void) {
    MarimayBytecodeProgram program = {0};
    MarimayBytecodeExecutionImage image = {0};
    MMAY_EXPECT(mmay_test_vm_build_nested_program(
        &program, mmay_system_allocator()));
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_execution_image_init(
        &image, &program, mmay_system_allocator()) == MMAY_BYTECODE_IMAGE_OK);
    MMAY_EXPECT(image.function_count == 3);
    const MarimayBytecodeImageOperation *middle =
        mmay_bytecode_image_operation_at(&image.functions[1], 0, 0);
    const MarimayBytecodeImageOperation *entry =
        mmay_bytecode_image_operation_at(&image.functions[2], 0, 0);
    MMAY_EXPECT(middle != NULL && middle->opcode == MMAY_OPCODE_CALL_DIRECT
        && middle->third == 0);
    MMAY_EXPECT(entry != NULL && entry->opcode == MMAY_OPCODE_CALL_DIRECT
        && entry->third == 1);
    MMAY_EXPECT(image.functions[0].borrows_caller_roots);
    MMAY_EXPECT(!image.functions[1].borrows_caller_roots);
    mmay_bytecode_execution_image_dispose(&image);
    mmay_bytecode_program_dispose(&program);
}

static void mmay_image_allocation_contract(
    const MarimayBytecodeProgram *program
) {
    for (size_t failure = 0; failure < 2; failure += 1) {
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayBytecodeExecutionImage image = {0};
        MarimayBytecodeImageStatus status = mmay_bytecode_execution_image_init(
            &image, program, mmay_fail_allocator(&state));
        if (failure == 0) {
            MMAY_EXPECT(status == MMAY_BYTECODE_IMAGE_ALLOCATION_FAILED);
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_IMAGE_OK);
        }
        mmay_bytecode_execution_image_dispose(&image);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

void mmay_test_vm_execution_image(void) {
    MarimayBytecodeProgram program = {0};
    MarimayBytecodeExecutionImage image = {0};
    MMAY_EXPECT(sizeof(MarimayBytecodeImageOperation) == 32);
    MMAY_EXPECT(mmay_image_follow_program(&program, false));
    MMAY_EXPECT(mmay_bytecode_execution_image_init(
        &image, &program, mmay_system_allocator()) == MMAY_BYTECODE_IMAGE_OK);
    MMAY_EXPECT(image.function_count == 1
        && image.functions[0].has_specialized_loop
        && image.functions[0].borrows_caller_roots);
    const MarimayBytecodeImageOperation *head =
        mmay_bytecode_image_operation_at(&image.functions[0], 1, 0);
    MMAY_EXPECT(head != NULL && head->fusion
        == MMAY_BYTECODE_IMAGE_FUSION_RECORD_ARRAY_FOLLOW_LOOP);
    mmay_bytecode_execution_image_dispose(&image);
    mmay_image_allocation_contract(&program);
    mmay_image_fuel_contract(&program);
    mmay_bytecode_program_dispose(&program);
    MMAY_EXPECT(mmay_image_follow_program(&program, true));
    MMAY_EXPECT(mmay_bytecode_execution_image_init(
        &image, &program, mmay_system_allocator()) == MMAY_BYTECODE_IMAGE_OK);
    head = mmay_bytecode_image_operation_at(&image.functions[0], 1, 0);
    MMAY_EXPECT(head != NULL && head->fusion
        == MMAY_BYTECODE_IMAGE_FUSION_RECORD_ARRAY_FOLLOW_LOOP);
    mmay_bytecode_execution_image_dispose(&image);
    mmay_image_fuel_contract(&program);
    mmay_bytecode_program_dispose(&program);
    mmay_image_direct_bindings();
}
