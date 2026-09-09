#include "test_bytecode_vm_support.h"

#include "mmay/runtime/array_view.h"

static bool mmay_image_shift_program(
    MarimayBytecodeProgram *program, bool alias_invariant
) {
    const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_ARRAY, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64
    };
    const MarimayBytecodeType locals[] = {
        MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_ARRAY, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_ARRAY, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_ARRAY, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64
    };
    MarimayBytecodeFunction function = {0};
    MarimaySymbolId symbol = {{0x51}};
    uint32_t layout = UINT32_MAX, id = UINT32_MAX;
    uint32_t blocks[5] = {
        UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
    };
    uint64_t span = 500;
    uint32_t copied_value = alias_invariant ? 4 : 15;
    bool ok = mmay_bytecode_function_init(&function,
            mmay_system_allocator(), MMAY_BYTECODE_TYPE_I64, 1,
            parameters, 3, mmay_bytecode_function_default_options())
        && mmay_test_vm_add_registers(
            &function, locals, sizeof(locals) / sizeof(locals[0]))
        && mmay_bytecode_add_array_type(&function,
            MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT, &layout)
        && layout == 0;
    for (size_t index = 0; ok && index < 5; index += 1) {
        ok = mmay_bytecode_add_block(&function, &blocks[index])
            && blocks[index] == index;
    }
    ok = ok && mmay_bytecode_set_register_layout(&function, 0, layout)
        && mmay_bytecode_set_register_layout(&function, 7, layout)
        && mmay_bytecode_set_register_layout(&function, 11, layout)
        && mmay_bytecode_set_register_layout(&function, 13, layout)
        && mmay_test_vm_append(
            &function, 0, MMAY_OPCODE_CONST_I64, 4, 0, 0, &span)
        && mmay_test_vm_append(
            &function, 0, MMAY_OPCODE_CONST_I64, 6, 1, 0, &span)
        && mmay_test_vm_append(
            &function, 0, MMAY_OPCODE_COPY, 17, 2, 0, &span)
        && mmay_test_vm_append(
            &function, 0, MMAY_OPCODE_JUMP, 1, 0, 0, &span)
        && mmay_test_vm_append(
            &function, 1, MMAY_OPCODE_I64_LESS, 3, 4, 17, &span)
        && mmay_test_vm_append(
            &function, 1, MMAY_OPCODE_BRANCH_BOOL, 3, 2, 4, &span)
        && mmay_test_vm_append(
            &function, 2, MMAY_OPCODE_I64_SUBTRACT_PROVEN, 5, 17, 6, &span)
        && mmay_test_vm_append(
            &function, 2, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 7, 0, 5, &span)
        && mmay_test_vm_append(
            &function, 2, MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, 8, 7, 5, &span)
        && mmay_test_vm_append(
            &function, 2, MMAY_OPCODE_I64_LESS, 9, 1, 8, &span)
        && mmay_test_vm_append(
            &function, 2, MMAY_OPCODE_BRANCH_BOOL, 9, 3, 4, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_COPY, 10, 17, 0, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 11, 0, 10, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_I64_SUBTRACT_PROVEN, 12, 17, 6, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 13, 0, 12, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, 14, 13, 12, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_ARRAY_SET_IN_BOUNDS, 11, 10, 14, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_COPY, copied_value, 14, 0, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_I64_SUBTRACT_PROVEN, 16, 17, 6, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_COPY, 17, 16, 0, &span)
        && mmay_test_vm_append(
            &function, 3, MMAY_OPCODE_JUMP, 1, 0, 0, &span)
        && mmay_test_vm_append(
            &function, 4, MMAY_OPCODE_RETURN, 17, 0, 0, &span)
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

static bool mmay_image_shift_array(
    MarimayBytecodeExecution *execution, MarimayBytecodeValue *array,
    MarimayBytecodeFunction *builder
) {
    uint32_t layout = UINT32_MAX, reg = UINT32_MAX, block = UINT32_MAX;
    MarimayBytecodeArrayConstructionId construction =
        MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
    uint64_t span = 600;
    bool built = mmay_bytecode_function_init(builder,
            mmay_system_allocator(), MMAY_BYTECODE_TYPE_ARRAY, 1, NULL, 0,
            mmay_bytecode_function_default_options())
        && mmay_bytecode_add_array_type(builder, MMAY_BYTECODE_TYPE_I64,
            MMAY_BYTECODE_NO_LAYOUT, &layout)
        && mmay_bytecode_add_register(
            builder, MMAY_BYTECODE_TYPE_ARRAY, &reg)
        && mmay_bytecode_set_register_layout(builder, reg, layout)
        && mmay_bytecode_add_array_construction_with_layout(builder,
            MMAY_BYTECODE_TYPE_ARRAY, layout, NULL, 0, &construction)
        && mmay_bytecode_add_block(builder, &block)
        && mmay_test_vm_append(builder, block,
            MMAY_OPCODE_ARRAY_NEW, reg, construction, 0, &span)
        && mmay_test_vm_append(
            builder, block, MMAY_OPCODE_RETURN, reg, 0, 0, &span);
    builder->result_layout = layout;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.execution = execution;
    bool ok = built && mmay_bytecode_run(builder, NULL, 0,
        execution->allocator, options, array) == MMAY_BYTECODE_RUN_OK;
    const int64_t initial[] = {9, 7, 5, 3};
    for (size_t index = 0;
        ok && index < sizeof(initial) / sizeof(initial[0]); index += 1) {
        MarimayManagedCell cell = {0};
        cell.f64_bits = (uint64_t)initial[index];
        ok = mmay_managed_array_push(&execution->managed_heap,
            array->as.managed_reference, cell) == MMAY_MANAGED_OK;
    }
    return ok;
}

typedef struct MarimayImageShiftObservation {
    MarimayBytecodeRunStatus status;
    MarimayBytecodeValue result;
    MarimayBytecodeTrap trap;
    MarimayBytecodeSpan panic_span;
    const char *panic_code;
    int64_t values[4];
    uint64_t generation;
    size_t remaining;
} MarimayImageShiftObservation;

static MarimayImageShiftObservation mmay_image_shift_observe(
    const MarimayBytecodeProgram *program, bool image, bool shared,
    size_t fuel, int64_t key, int64_t loop, bool saturated
) {
    MarimayImageShiftObservation observed = {0};
    MarimayFailAllocator state;
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeFunction builder = {0};
    MarimayBytecodePanic panic = {0};
    MarimayBytecodeValue arguments[] = {
        {0}, mmay_test_vm_i64(key), mmay_test_vm_i64(loop)
    };
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, allocator,
        mmay_bytecode_execution_default_options()));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, allocator));
    MMAY_EXPECT(mmay_image_shift_array(&execution, &arguments[0], &builder));
    const MarimayManagedArrayDescriptor *descriptor = NULL;
    MarimayManagedArrayView view;
    MMAY_EXPECT(mmay_managed_array_describe(&execution.managed_heap,
        arguments[0].as.managed_reference, &descriptor) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_view_bind(&execution.managed_heap,
        arguments[0].as.managed_reference, descriptor, &view)
        == MMAY_MANAGED_OK);
    if (saturated) *view.mutation_generation = UINT64_MAX;
    observed.remaining = fuel;
    observed.result = mmay_test_vm_i64(1234);
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.execution = &execution;
    options.panic = &panic;
    options.trap = &observed.trap;
    options.max_steps = shared ? SIZE_MAX : fuel;
    options.remaining_shared_steps = shared ? &observed.remaining : NULL;
    observed.status = image
        ? mmay_bytecode_run_program(program, arguments, 3, allocator,
            options, &observed.result)
        : mmay_bytecode_run(
            &mmay_bytecode_program_function(program, 0)->body,
            arguments, 3, allocator, options, &observed.result);
    observed.panic_code = panic.code;
    observed.panic_span = panic.span;
    size_t length = 0;
    for (size_t index = 0; index < 4; index += 1) {
        MarimayManagedCell cell = {0};
        MMAY_EXPECT(mmay_managed_array_get(&execution.managed_heap,
            arguments[0].as.managed_reference, (int64_t)index,
            &cell, &length) == MMAY_MANAGED_OK);
        observed.values[index] = (int64_t)cell.f64_bits;
    }
    observed.generation = *view.mutation_generation;
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_function_dispose(&builder);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    return observed;
}

static void mmay_image_shift_compare(
    MarimayImageShiftObservation cold,
    MarimayImageShiftObservation image
) {
    MMAY_EXPECT(cold.status == image.status);
    MMAY_EXPECT(cold.result.type == image.result.type);
    MMAY_EXPECT(cold.result.as.i64 == image.result.as.i64);
    MMAY_EXPECT(cold.remaining == image.remaining);
    MMAY_EXPECT(cold.generation == image.generation);
    MMAY_EXPECT(memcmp(cold.values, image.values, sizeof(cold.values)) == 0);
    MMAY_EXPECT(cold.trap.has_span == image.trap.has_span);
    if (cold.trap.has_span) {
        MMAY_EXPECT(cold.trap.span.start == image.trap.span.start);
        MMAY_EXPECT(cold.trap.span.end == image.trap.span.end);
    }
    MMAY_EXPECT((cold.panic_code == NULL) == (image.panic_code == NULL));
    if (cold.panic_code != NULL && image.panic_code != NULL) {
        MMAY_EXPECT(strcmp(cold.panic_code, image.panic_code) == 0);
        MMAY_EXPECT(cold.panic_span.start == image.panic_span.start);
        MMAY_EXPECT(cold.panic_span.end == image.panic_span.end);
    }
}

void mmay_test_vm_execution_image_shift(void) {
    MarimayBytecodeProgram program = {0};
    MMAY_EXPECT(mmay_image_shift_program(&program, false));
    for (size_t fuel = 0; fuel <= 62; fuel += 1) {
        for (unsigned shared = 0; shared < 2; shared += 1) {
            mmay_image_shift_compare(
                mmay_image_shift_observe(
                    &program, false, shared != 0, fuel, 0, 3, false),
                mmay_image_shift_observe(
                    &program, true, shared != 0, fuel, 0, 3, false));
        }
    }
    mmay_image_shift_compare(
        mmay_image_shift_observe(&program, false, false, 100, 10, 3, false),
        mmay_image_shift_observe(&program, true, false, 100, 10, 3, false));
    mmay_image_shift_compare(
        mmay_image_shift_observe(&program, false, false, 100, 0, 5, false),
        mmay_image_shift_observe(&program, true, false, 100, 0, 5, false));
    mmay_image_shift_compare(
        mmay_image_shift_observe(&program, false, false, 100, 0, 3, true),
        mmay_image_shift_observe(&program, true, false, 100, 0, 3, true));
    mmay_bytecode_program_dispose(&program);

    MMAY_EXPECT(mmay_image_shift_program(&program, true));
    mmay_image_shift_compare(
        mmay_image_shift_observe(&program, false, false, 100, 0, 3, false),
        mmay_image_shift_observe(&program, true, false, 100, 0, 3, false));
    mmay_bytecode_program_dispose(&program);
}
