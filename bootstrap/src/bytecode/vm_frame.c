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
MarimayBytecodeRunStatus mmay_bytecode_run_frame_init(
    MarimayBytecodeRunFrame *frame,
    const MarimayBytecodeProgramFunction *function,
    const MarimayBytecodeValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    uint32_t return_destination,
    bool return_is_void,
    bool return_is_never,
    MarimayBytecodeSpan call_span
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
        if (!mmay_size_multiply(
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
        memset(frame->registers, 0, register_bytes);
        void *defers = mmay_bytecode_frame_reserve_buffer(frame->defer_stack,
            &frame->defer_stack_bytes, defer_bytes, allocator);
        if (defer_bytes != 0 && defers == NULL) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        frame->defer_stack = defers;
        for (index = 0; index < body->register_types.length; index += 1) {
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
MarimayBytecodeRunStatus mmay_bytecode_run_frame_roots_init(
    MarimayBytecodeRunFrame *frame,
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator
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
    if (!mmay_bytecode_run_function_uses_managed(&frame->function->body)) {
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
    size_t count = 0;
    const MarimayBytecodeFunction *body = &frame->function->body;
    const MarimayBytecodeType *types = body->register_types.data;
    for (size_t index = 0; index < body->register_types.length; index += 1) {
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
    count = 0;
    for (size_t index = 0; index < body->register_types.length; index += 1) {
        if (mmay_bytecode_type_is_managed(types[index])) {
            frame->managed_roots->locations[count++] =
                &frame->registers[index].as.managed_reference;
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
