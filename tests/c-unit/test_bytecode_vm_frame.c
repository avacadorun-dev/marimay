#include "test_bytecode_vm_support.h"
#include "vm_frame.h"

static bool frame_initialize(MarimayBytecodeRunFrame *frame,
    const MarimayBytecodeProgramFunction *function,
    MarimayBytecodeExecution *execution, MarimayAllocator allocator) {
    MarimayBytecodeRunStatus status = mmay_bytecode_run_frame_init(frame,
        function, NULL, 0, allocator, 0, false, false, (MarimayBytecodeSpan){0});
    if (status == MMAY_BYTECODE_RUN_OK) {
        status = mmay_bytecode_run_frame_roots_init(frame, execution, allocator);
    }
    MMAY_EXPECT(status == MMAY_BYTECODE_RUN_OK
        || status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
    return status == MMAY_BYTECODE_RUN_OK;
}

static bool frame_lifecycle(const MarimayBytecodeProgramFunction *managed,
    const MarimayBytecodeProgramFunction *scalar, MarimayFailAllocator *state) {
    MarimayAllocator allocator = mmay_fail_allocator(state);
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeRunFrame *frames = NULL;
    size_t capacity = 0;
    bool completed = false;
    if (!mmay_bytecode_execution_init(&execution, allocator,
            mmay_bytecode_execution_default_options())) goto cleanup;
    MMAY_EXPECT(!mmay_bytecode_run_frames_reserve(&frames, NULL, 1, 4, allocator));
    if (!mmay_bytecode_run_frames_reserve(&frames, &capacity, 1, 4, allocator)
        || !frame_initialize(&frames[0], managed, &execution, allocator)) goto cleanup;
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
    if (!frame_initialize(&frames[0], managed, &execution, allocator)) goto cleanup;
    MMAY_EXPECT(state->attempts == attempts);
    MMAY_EXPECT(mmay_managed_reference_is_invalid(frames[0].registers[0].as.managed_reference));
    MMAY_EXPECT(mmay_managed_reference_is_invalid(frames[0].registers[1].as.managed_reference));
    MMAY_EXPECT(frames[0].block == 0 && frames[0].instruction == 0
        && frames[0].steps == 0 && frames[0].defer_count == 0);
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    /* A different callee replaces the inactive layout. Exercise failures in
     * both replacements and teardown of inactive slots beyond active depth. */
    if (!frame_initialize(&frames[0], scalar, &execution, allocator)) goto cleanup;
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    attempts = state->attempts;
    if (!frame_initialize(&frames[0], scalar, &execution, allocator)) goto cleanup;
    MMAY_EXPECT(state->attempts == attempts);
    mmay_bytecode_run_frame_release(&frames[0], &execution);
    if (!frame_initialize(&frames[0], managed, &execution, allocator)) goto cleanup;
    if (!frame_initialize(&frames[3], managed, &execution, allocator)) goto cleanup;
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
    MarimayBytecodeProgramFunction managed = {0}, scalar = {0};
    MMAY_EXPECT(mmay_test_vm_build_string_identity(&managed.body, mmay_system_allocator()));
    const MarimayBytecodeType local[] = {MMAY_BYTECODE_TYPE_STRING};
    MMAY_EXPECT(mmay_test_vm_add_registers(&managed.body, local, 1));
    MMAY_EXPECT(mmay_test_vm_build_direct_constant(&scalar.body,
        mmay_system_allocator(), 17, 100));
    bool completed = false;
    for (size_t failure = 0; failure < 128 && !completed; failure += 1) {
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        completed = frame_lifecycle(&managed, &scalar, &state);
    }
    MMAY_EXPECT(completed);
    mmay_bytecode_function_dispose(&managed.body);
    mmay_bytecode_function_dispose(&scalar.body);
}
