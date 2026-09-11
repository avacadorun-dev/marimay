#include "test_bytecode_vm_support.h"
#include "vm_frame.h"
#include "bytecode_internal.h"

static bool frame_initialize(MarimayBytecodeRunFrame *frame,
    const MarimayBytecodeProgramFunction *function,
    MarimayBytecodeExecution *execution, MarimayAllocator allocator,
    const MarimayBytecodeFrameLayout *layouts) {
#ifdef MMAY_TEST_HAS_SLJIT
    const MarimayBytecodeFrameLayout *layout = NULL;
    for (size_t i = 0; layouts != NULL && i < 3; ++i)
        if (layouts[i].function == function) layout = &layouts[i];
    MMAY_EXPECT(layouts == NULL || layout != NULL);
    MarimayBytecodeRunStatus status = layout != NULL
        ? mmay_bytecode_run_frame_init_layout(frame, layout, allocator,
            0, false, false, (MarimayBytecodeSpan){0}) : mmay_bytecode_run_frame_init(frame,
        function, NULL, 0, allocator, 0, false, false, (MarimayBytecodeSpan){0});
    if (status == MMAY_BYTECODE_RUN_OK) {
        status = layout != NULL
            ? mmay_bytecode_run_frame_roots_init_layout(frame, layout, execution, allocator)
            : mmay_bytecode_run_frame_roots_init(frame, execution, allocator);
    }
#else
    MMAY_EXPECT(layouts == NULL);
    MarimayBytecodeRunStatus status = mmay_bytecode_run_frame_init(frame,
        function, NULL, 0, allocator, 0, false, false, (MarimayBytecodeSpan){0});
    if (status == MMAY_BYTECODE_RUN_OK)
        status = mmay_bytecode_run_frame_roots_init(frame, execution, allocator);
#endif
    MMAY_EXPECT(status == MMAY_BYTECODE_RUN_OK
        || status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
    return status == MMAY_BYTECODE_RUN_OK;
}

static bool frame_lifecycle(const MarimayBytecodeProgramFunction *managed,
    const MarimayBytecodeProgramFunction *scalar,
    const MarimayBytecodeProgramFunction *larger, MarimayFailAllocator *state,
    const MarimayBytecodeFrameLayout *layouts) {
    MarimayAllocator allocator = mmay_fail_allocator(state);
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeRunFrame *frames = NULL;
    size_t capacity = 0;
    bool completed = false;
    if (!mmay_bytecode_execution_init(&execution, allocator,
            mmay_bytecode_execution_default_options())) goto cleanup;
    MMAY_EXPECT(!mmay_bytecode_run_frames_reserve(&frames, NULL, 1, 4, allocator));
    if (!mmay_bytecode_run_frames_reserve(&frames, &capacity, 1, 4, allocator)
        || !frame_initialize(&frames[0], managed, &execution, allocator, layouts)) goto cleanup;
    MarimayManagedStatus status = mmay_managed_string_new(&execution.managed_heap,
        (const unsigned char *)"retained", 8, &frames[0].registers[0].as.managed_reference);
    MMAY_EXPECT(status == MMAY_MANAGED_OK || status == MMAY_MANAGED_ALLOCATION_FAILED);
    if (status != MMAY_MANAGED_OK) goto cleanup;
    frames[0].registers[1] = frames[0].registers[0];
    /* Moving the frame vector must leave the linked root frame and locations
     * valid. Otherwise this collection loses the live string. */
    if (!mmay_bytecode_run_frames_reserve(&frames, &capacity, 4, 4, allocator)) goto cleanup;
    MMAY_EXPECT(mmay_managed_heap_collect(&execution.managed_heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&execution.managed_heap) == 1);
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    MMAY_EXPECT(mmay_managed_heap_collect(&execution.managed_heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&execution.managed_heap) == 0);
    size_t attempts = state->attempts;
    frames[0].block = 7;
    frames[0].instruction = 9;
    frames[0].steps = 13;
    frames[0].defer_count = 1;
    /* Both stale references must be cleared before checked root registration;
     * same-function reuse must perform no allocation attempts. */
    if (!frame_initialize(&frames[0], managed, &execution, allocator, layouts)) goto cleanup;
    MMAY_EXPECT(state->attempts == attempts);
    MMAY_EXPECT(mmay_managed_reference_is_invalid(frames[0].registers[0].as.managed_reference));
    MMAY_EXPECT(mmay_managed_reference_is_invalid(frames[0].registers[1].as.managed_reference));
    MMAY_EXPECT(frames[0].block == 0 && frames[0].instruction == 0
        && frames[0].steps == 0 && frames[0].defer_count == 0);
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    /* Different callees rebuild their types and root locations in the retained
     * capacity. Smaller layouts must not register stale trailing locations. */
    if (!frame_initialize(&frames[0], scalar, &execution, allocator, layouts)) goto cleanup;
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    attempts = state->attempts;
    if (!frame_initialize(&frames[0], scalar, &execution, allocator, layouts)) goto cleanup;
    MMAY_EXPECT(state->attempts == attempts);
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    if (!frame_initialize(&frames[0], managed, &execution, allocator, layouts)) goto cleanup;
    MMAY_EXPECT(mmay_bytecode_run_frame_init(&frames[0], scalar, NULL, 0,
        allocator, 0, false, false, (MarimayBytecodeSpan){0})
        == MMAY_BYTECODE_RUN_INVALID_BYTECODE);
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    if (!frame_initialize(&frames[0], larger, &execution, allocator, layouts)) goto cleanup;
    MMAY_EXPECT(frames[0].register_bytes >= 6 * sizeof(*frames[0].registers));
    MMAY_EXPECT(frames[0].defer_stack_bytes >= sizeof(*frames[0].defer_stack));
    status = mmay_managed_string_new(&execution.managed_heap,
        (const unsigned char *)"large", 5,
        &frames[0].registers[1].as.managed_reference);
    MMAY_EXPECT(status == MMAY_MANAGED_OK || status == MMAY_MANAGED_ALLOCATION_FAILED);
    if (status != MMAY_MANAGED_OK) goto cleanup;
    frames[0].registers[3] = frames[0].registers[1];
    frames[0].registers[5] = frames[0].registers[1];
    MMAY_EXPECT(mmay_managed_heap_collect(&execution.managed_heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&execution.managed_heap) == 1);
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    MMAY_EXPECT(mmay_managed_heap_collect(&execution.managed_heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&execution.managed_heap) == 0);
    attempts = state->attempts;
    for (size_t repeat = 0; repeat < 16; repeat += 1) {
        const MarimayBytecodeProgramFunction *sequence[] = {scalar, managed, larger};
        for (size_t choice = 0; choice < 3; choice += 1) {
            const MarimayBytecodeProgramFunction *function = sequence[choice];
            if (!frame_initialize(&frames[0], function, &execution, allocator, layouts)) goto cleanup;
            const MarimayBytecodeType *types = function->body.register_types.data;
            for (size_t index = 0; index < function->body.register_types.length; index += 1) {
                MMAY_EXPECT(frames[0].registers[index].type == types[index]);
                if (types[index] == MMAY_BYTECODE_TYPE_STRING) {
                    MMAY_EXPECT(mmay_managed_reference_is_invalid(
                        frames[0].registers[index].as.managed_reference));
                }
            }
            MMAY_EXPECT(mmay_managed_heap_collect(&execution.managed_heap) == MMAY_MANAGED_OK);
            MMAY_EXPECT(state->attempts == attempts);
            mmay_bytecode_run_frame_release(&frames[0], &execution);
        }
    }
    /* Borrowing after a layout change must leave the retained root frame
     * unlinked; the next owning layout rebuilds the complete location map. */
    if (mmay_bytecode_run_frame_init(&frames[0], managed, NULL, 0, allocator,
            0, false, false, (MarimayBytecodeSpan){0}) != MMAY_BYTECODE_RUN_OK) goto cleanup;
    mmay_bytecode_run_frame_borrow_caller_roots(&frames[0]);
    MMAY_EXPECT(mmay_managed_heap_collect(&execution.managed_heap) == MMAY_MANAGED_OK);
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    if (!frame_initialize(&frames[0], larger, &execution, allocator, layouts)) goto cleanup;
    MMAY_EXPECT(state->attempts == attempts);
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    if (!frame_initialize(&frames[3], managed, &execution, allocator, layouts)) goto cleanup;
    mmay_bytecode_run_frame_release(&frames[3], &execution);
    completed = true;
cleanup:
    if (frames != NULL) {
        for (size_t index = capacity; index != 0; index -= 1) {
            mmay_bytecode_run_frame_release(&frames[index - 1], &execution);
        }
    }
    mmay_bytecode_run_frames_dispose(frames, capacity, &allocator, &execution);
    mmay_bytecode_execution_dispose(&execution);
    MMAY_EXPECT(state->live_allocations == 0 && state->live_bytes == 0);
    return completed;
}

void mmay_test_vm_frame_storage(void) {
    MarimayBytecodeProgramFunction managed = {0}, scalar = {0}, larger = {0};
    MMAY_EXPECT(mmay_test_vm_build_string_identity(&managed.body, mmay_system_allocator()));
    const MarimayBytecodeType local[] = {MMAY_BYTECODE_TYPE_STRING};
    MMAY_EXPECT(mmay_test_vm_add_registers(&managed.body, local, 1));
    MMAY_EXPECT(mmay_test_vm_build_direct_constant(&scalar.body,
        mmay_system_allocator(), 17, 100));
    MMAY_EXPECT(mmay_test_vm_build_direct_forward(&larger.body,
        mmay_system_allocator(), (MarimaySymbolId){{0x55}}, 200));
    const MarimayBytecodeType extra[] = {MMAY_BYTECODE_TYPE_STRING,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_STRING,
        MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_STRING};
    MMAY_EXPECT(mmay_test_vm_add_registers(&larger.body, extra, 5));
#ifdef MMAY_TEST_HAS_SLJIT
    const MarimayBytecodeProgramFunction *functions[] = {&managed, &scalar, &larger};
    MarimayBytecodeFrameLayout layouts[3] = {0};
    void *storage[3] = {0};
    size_t bytes[3] = {0};
    MarimayAllocator allocator = mmay_system_allocator();
    for (size_t i = 0; i < 3; ++i) {
        MMAY_EXPECT(mmay_bytecode_frame_layout_size(functions[i], &bytes[i]));
        storage[i] = mmay_allocate(&allocator, bytes[i]);
        MMAY_EXPECT(storage[i] != NULL);
        MMAY_EXPECT(!mmay_bytecode_frame_layout_init(functions[i], storage[i], bytes[i] - 1, &layouts[i]));
        MMAY_EXPECT(mmay_bytecode_frame_layout_init(functions[i], storage[i], bytes[i], &layouts[i]));
        MMAY_EXPECT(layouts[i].function == functions[i]);
    }
    for (size_t prepared = 0; prepared < 2; ++prepared) {
        bool completed = false;
        for (size_t failure = 0; failure < 128 && !completed; failure += 1) {
            MarimayFailAllocator state;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            completed = frame_lifecycle(&managed, &scalar, &larger, &state,
                prepared ? layouts : NULL);
        }
        MMAY_EXPECT(completed);
    }
    MarimayBytecodeRunFrame empty = {0};
    MMAY_EXPECT(mmay_bytecode_run_frame_init_layout(&empty, NULL, allocator,
        0, false, false, (MarimayBytecodeSpan){0}) == MMAY_BYTECODE_RUN_INVALID_BYTECODE);
    MMAY_EXPECT(mmay_bytecode_run_frame_roots_init_layout(&empty, &layouts[0], NULL,
        allocator) == MMAY_BYTECODE_RUN_INVALID_BYTECODE);
    for (size_t i = 0; i < 3; ++i) {
        const MarimayBytecodeType *types = functions[i]->body.register_types.data;
        for (size_t reg = 0; reg < functions[i]->body.register_types.length; ++reg) {
            MMAY_EXPECT(layouts[i].initial_registers[reg].type == types[reg]);
            if (mmay_bytecode_type_is_managed(types[reg]))
                MMAY_EXPECT(mmay_managed_reference_is_invalid(
                    layouts[i].initial_registers[reg].as.managed_reference));
        }
        mmay_deallocate(&allocator, storage[i], bytes[i]);
    }
#else
    bool completed = false;
    for (size_t failure = 0; failure < 128 && !completed; ++failure) {
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        completed = frame_lifecycle(&managed, &scalar, &larger, &state, NULL);
    }
    MMAY_EXPECT(completed);
#endif
    mmay_bytecode_function_dispose(&managed.body);
    mmay_bytecode_function_dispose(&scalar.body);
    mmay_bytecode_function_dispose(&larger.body);
}
