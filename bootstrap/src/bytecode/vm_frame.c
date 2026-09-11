#include "vm_frame.h"
#include "vm_internal.h"

#include <string.h>

/* Only inactive buffers may grow. Preserve the previous allocation on failure
 * so the normal frame teardown still owns every byte. Contents are rebuilt. */
static void *mmay_bytecode_frame_reserve_buffer(void *buffer, size_t *capacity,
    size_t required, MarimayAllocator allocator) {
    if (required <= *capacity) return buffer;
    void *next = mmay_allocate(&allocator, required);
    if (next == NULL) return NULL;
    mmay_deallocate(&allocator, buffer, *capacity);
    *capacity = required;
    return next;
}

void mmay_bytecode_run_frame_dispose(
    MarimayBytecodeRunFrame *frame,
    const MarimayAllocator *allocator,
    MarimayBytecodeExecution *execution
) {
    if (frame != NULL && allocator != NULL) {
        if (frame->managed_roots != NULL) {
            frame->managed_roots->location_bytes = frame->root_location_capacity;
            mmay_bytecode_run_roots_dispose(
                frame->managed_roots, execution, *allocator
            );
            mmay_deallocate(
                allocator, frame->managed_roots,
                sizeof(*frame->managed_roots)
            );
        }
        mmay_deallocate(
            allocator, frame->defer_stack, frame->defer_stack_bytes
        );
        mmay_deallocate(allocator, frame->registers, frame->register_bytes);
        memset(frame, 0, sizeof(*frame));
    }
}
#ifdef MMAY_HAVE_SLJIT
static MarimayBytecodeRunStatus initialize(
#else
MarimayBytecodeRunStatus mmay_bytecode_run_frame_init(
#endif
    MarimayBytecodeRunFrame *frame,
    const MarimayBytecodeProgramFunction *function,
    const MarimayBytecodeValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    uint32_t return_destination,
    bool return_is_void,
    bool return_is_never,
#ifdef MMAY_HAVE_SLJIT
    MarimayBytecodeSpan call_span,
    const MarimayBytecodeFrameLayout *layout
#else
    MarimayBytecodeSpan call_span
#endif
) {
    const MarimayBytecodeFunction *body = function != NULL ? &function->body : NULL;
    size_t index = 0;
    if (frame == NULL || body == NULL
        || argument_count > body->parameter_count
        || (arguments == NULL && argument_count != 0)
        || (frame->managed_roots != NULL && frame->managed_roots->frame.linked)) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (frame->function == function && frame->roots_prepared) {
        /* Inactive buffers are not roots. Clear every old reference before
         * relinking; scalar definitions are guaranteed by the verifier. */
        if (frame->managed_roots != NULL) {
            size_t count = frame->managed_roots->location_bytes
                / sizeof(*frame->managed_roots->locations);
            for (index = 0; index < count; index += 1) {
                *frame->managed_roots->locations[index] = mmay_managed_invalid_reference();
            }
        }
    } else {
        size_t register_bytes = 0;
        size_t defer_bytes = 0;
        frame->function = NULL;
        frame->roots_prepared = false;
        if (frame->managed_roots != NULL) {
            /* Old locations may refer to a smaller layout or a buffer about
             * to move. They are inactive and must never be relinked. */
            frame->managed_roots->location_bytes = 0;
        }
#ifdef MMAY_HAVE_SLJIT
        if (layout != NULL) {
            register_bytes = layout->register_bytes;
            defer_bytes = layout->defer_bytes;
        } else if (!mmay_size_multiply(
#else
        if (!mmay_size_multiply(
#endif
                body->register_types.length == 0 ? 1 : body->register_types.length,
                sizeof(MarimayBytecodeValue), &register_bytes)
            || !mmay_size_multiply(body->calls.length,
                sizeof(*frame->defer_stack), &defer_bytes)) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        void *registers = mmay_bytecode_frame_reserve_buffer(frame->registers,
            &frame->register_bytes, register_bytes, allocator);
        if (registers == NULL) return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        frame->registers = registers;
#ifdef MMAY_HAVE_SLJIT
        if (layout != NULL) memcpy(frame->registers, layout->initial_registers, register_bytes);
        else memset(frame->registers, 0, register_bytes);
#else
        memset(frame->registers, 0, register_bytes);
#endif
        void *defers = mmay_bytecode_frame_reserve_buffer(frame->defer_stack,
            &frame->defer_stack_bytes, defer_bytes, allocator);
        if (defer_bytes != 0 && defers == NULL) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        frame->defer_stack = defers;
#ifdef MMAY_HAVE_SLJIT
        for (index = 0; layout == NULL && index < body->register_types.length; index += 1) {
#else
        for (index = 0; index < body->register_types.length; index += 1) {
#endif
            const MarimayBytecodeType *type = mmay_vector_at_const(
                &body->register_types,
                index
            );
            if (type == NULL) {
                mmay_bytecode_run_frame_dispose(frame, &allocator, NULL);
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            }
            frame->registers[index].type = *type;
            if (mmay_bytecode_type_is_managed(*type)) {
                frame->registers[index].as.managed_reference =
                    mmay_managed_invalid_reference();
            }
        }
    }
    for (index = 0; index < argument_count; index += 1) {
        frame->registers[index] = arguments[index];
    }
    frame->block = 0;
    frame->instruction = 0;
    frame->steps = 0;
    frame->defer_count = 0;
    frame->function = function;
    frame->return_destination = return_destination;
    frame->return_is_void = return_is_void;
    frame->return_is_never = return_is_never;
    frame->call_span = call_span;
    return MMAY_BYTECODE_RUN_OK;
}
#ifdef MMAY_HAVE_SLJIT
static MarimayBytecodeRunStatus initialize_roots(
#else
MarimayBytecodeRunStatus mmay_bytecode_run_frame_roots_init(
#endif
    MarimayBytecodeRunFrame *frame,
    MarimayBytecodeExecution *execution,
#ifdef MMAY_HAVE_SLJIT
    MarimayAllocator allocator,
    const MarimayBytecodeFrameLayout *layout
#else
    MarimayAllocator allocator
#endif
) {
    MarimayBytecodeRunStatus status;
    if (frame == NULL || frame->function == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (frame->roots_prepared) {
        if (frame->managed_roots == NULL) return MMAY_BYTECODE_RUN_OK;
        if (execution == NULL) return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
        MarimayManagedStatus managed_status = mmay_managed_root_push(
            &execution->managed_heap, &frame->managed_roots->frame,
            frame->managed_roots->locations, frame->managed_roots->location_bytes
                / sizeof(*frame->managed_roots->locations));
        return managed_status == MMAY_MANAGED_OK ? MMAY_BYTECODE_RUN_OK
            : managed_status == MMAY_MANAGED_INVALID_REFERENCE
                ? MMAY_BYTECODE_RUN_INVALID_REFERENCE : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
#ifdef MMAY_HAVE_SLJIT
    if (layout != NULL ? layout->managed_count == 0
        : !mmay_bytecode_run_function_uses_managed(&frame->function->body)) {
#else
    if (!mmay_bytecode_run_function_uses_managed(&frame->function->body)) {
#endif
        frame->roots_prepared = true;
        return MMAY_BYTECODE_RUN_OK;
    }
    if (execution == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (frame->managed_roots == NULL) {
        frame->managed_roots = mmay_allocate(&allocator, sizeof(*frame->managed_roots));
        if (frame->managed_roots == NULL) return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(frame->managed_roots, 0, sizeof(*frame->managed_roots));
    }
#ifdef MMAY_HAVE_SLJIT
    size_t count = layout != NULL ? layout->managed_count : 0;
#else
    size_t count = 0;
#endif
    const MarimayBytecodeFunction *body = &frame->function->body;
    const MarimayBytecodeType *types = body->register_types.data;
#ifdef MMAY_HAVE_SLJIT
    for (size_t index = 0; layout == NULL && index < body->register_types.length; index += 1) {
#else
    for (size_t index = 0; index < body->register_types.length; index += 1) {
#endif
        if (mmay_bytecode_type_is_managed(types[index])) count += 1;
    }
    size_t bytes = 0;
    if (!mmay_size_multiply(count, sizeof(*frame->managed_roots->locations), &bytes)) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    void *locations = mmay_bytecode_frame_reserve_buffer(frame->managed_roots->locations,
        &frame->root_location_capacity, bytes, allocator);
    if (bytes != 0 && locations == NULL) return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    frame->managed_roots->locations = locations;
    frame->managed_roots->location_bytes = bytes;
#ifdef MMAY_HAVE_SLJIT
    if (layout != NULL) {
        for (size_t i = 0; i < count; ++i)
            frame->managed_roots->locations[i] =
                &frame->registers[layout->managed_registers[i]].as.managed_reference;
    } else {
        count = 0;
        for (size_t index = 0; index < body->register_types.length; index += 1) {
            if (mmay_bytecode_type_is_managed(types[index])) {
                frame->managed_roots->locations[count++] =
                    &frame->registers[index].as.managed_reference;
            }
#else
    count = 0;
    for (size_t index = 0; index < body->register_types.length; index += 1) {
        if (mmay_bytecode_type_is_managed(types[index])) {
            frame->managed_roots->locations[count++] =
                &frame->registers[index].as.managed_reference;
#endif
        }
    }
    MarimayManagedStatus managed_status = mmay_managed_root_push(
        &execution->managed_heap, &frame->managed_roots->frame,
        frame->managed_roots->locations, count);
    status = managed_status == MMAY_MANAGED_OK ? MMAY_BYTECODE_RUN_OK
        : managed_status == MMAY_MANAGED_INVALID_REFERENCE
            ? MMAY_BYTECODE_RUN_INVALID_REFERENCE : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    if (status == MMAY_BYTECODE_RUN_OK) frame->roots_prepared = true;
    return status;
}

#ifdef MMAY_HAVE_SLJIT
MarimayBytecodeRunStatus mmay_bytecode_run_frame_init(
    MarimayBytecodeRunFrame *frame, const MarimayBytecodeProgramFunction *function,
    const MarimayBytecodeValue *arguments, size_t argument_count,
    MarimayAllocator allocator, uint32_t destination, bool returns_void,
    bool returns_never, MarimayBytecodeSpan span) {
    return initialize(frame, function, arguments, argument_count, allocator,
        destination, returns_void, returns_never, span, NULL);
}

MarimayBytecodeRunStatus mmay_bytecode_run_frame_init_layout(
    MarimayBytecodeRunFrame *frame, const MarimayBytecodeFrameLayout *layout,
    MarimayAllocator allocator, uint32_t destination, bool returns_void,
    bool returns_never, MarimayBytecodeSpan span) {
    if (layout == NULL || layout->function == NULL || layout->initial_registers == NULL)
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    return initialize(frame, layout->function, NULL, 0, allocator,
        destination, returns_void, returns_never, span, layout);
}

MarimayBytecodeRunStatus mmay_bytecode_run_frame_roots_init(
    MarimayBytecodeRunFrame *frame, MarimayBytecodeExecution *execution,
    MarimayAllocator allocator) {
    return initialize_roots(frame, execution, allocator, NULL);
}

MarimayBytecodeRunStatus mmay_bytecode_run_frame_roots_init_layout(
    MarimayBytecodeRunFrame *frame, const MarimayBytecodeFrameLayout *layout,
    MarimayBytecodeExecution *execution, MarimayAllocator allocator) {
    if (frame == NULL || layout == NULL || frame->function != layout->function)
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    return initialize_roots(frame, execution, allocator, layout);
}
#endif

void mmay_bytecode_run_frame_borrow_caller_roots(
    MarimayBytecodeRunFrame *frame
) {
    if (frame != NULL) frame->roots_prepared = true;
}
bool mmay_bytecode_run_frames_reserve(
    MarimayBytecodeRunFrame **frames,
    size_t *capacity,
    size_t required,
    size_t maximum,
    MarimayAllocator allocator
) {
    MarimayBytecodeRunFrame *next = NULL;
    size_t next_capacity = 0;
    size_t bytes = 0;
    if (frames == NULL || capacity == NULL || required > maximum) {
        return false;
    }
    next_capacity = *capacity == 0 ? 1 : *capacity;
    if (*capacity >= required) {
        return true;
    }
    while (next_capacity < required) {
        if (next_capacity > maximum / 2) {
            next_capacity = maximum;
        } else {
            next_capacity *= 2;
        }
    }
    if (!mmay_size_multiply(
            next_capacity,
            sizeof(MarimayBytecodeRunFrame),
            &bytes
        )) {
        return false;
    }
    next = mmay_allocate(&allocator, bytes);
    if (next == NULL) {
        return false;
    }
    memset(next, 0, bytes);
    if (*frames != NULL) {
        size_t old_bytes = 0;
        if (!mmay_size_multiply(
                *capacity,
                sizeof(MarimayBytecodeRunFrame),
                &old_bytes
            )) {
            mmay_deallocate(&allocator, next, bytes);
            return false;
        }
        memcpy(next, *frames, old_bytes);
        mmay_deallocate(&allocator, *frames, old_bytes);
    }
    *frames = next;
    *capacity = next_capacity;
    return true;
}

void mmay_bytecode_run_frame_release(MarimayBytecodeRunFrame *frame,
    MarimayBytecodeExecution *execution) {
    if (frame != NULL && frame->managed_roots != NULL && execution != NULL
        && frame->managed_roots->frame.linked) {
        (void)mmay_managed_root_pop(&execution->managed_heap,
            &frame->managed_roots->frame);
    }
}

void mmay_bytecode_run_frames_dispose(MarimayBytecodeRunFrame *frames,
    size_t capacity, const MarimayAllocator *allocator,
    MarimayBytecodeExecution *execution) {
    if (frames == NULL) return;
    for (size_t index = 0; index < capacity; index += 1) {
        mmay_bytecode_run_frame_dispose(&frames[index], allocator, execution);
    }
    size_t bytes = 0;
    if (mmay_size_multiply(capacity, sizeof(*frames), &bytes)) {
        mmay_deallocate(allocator, frames, bytes);
    }
}
