#include "vm_activation.h"
#include "vm_internal.h"
#include <string.h>

#ifdef MMAY_HAVE_SLJIT
static MarimayBytecodeRunStatus call_transition(
#else
MarimayBytecodeRunStatus mmay_bytecode_run_call(
#endif
    MarimayBytecodeRunStack *stack, const MarimayBytecodeProgram *program,
    const MarimayBytecodeExecutionImage *image,
    const MarimayBytecodeInstruction *instruction, MarimayAllocator allocator,
#ifdef MMAY_HAVE_SLJIT
    const MarimayBytecodeRunOptions *options,
    const MarimayBytecodePreparedCall *prepared
#else
    const MarimayBytecodeRunOptions *options
#endif
) {
    const size_t count = stack->count;
    MarimayBytecodeRunFrame *frame = &stack->frames[count - 1];
    const MarimayBytecodeFunction *body = &frame->function->body;
    MarimayBytecodeRunStatus status = MMAY_BYTECODE_RUN_OK;
    bool returns_never = instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER;
    bool interface_call = instruction->opcode
        == MMAY_OPCODE_CALL_INTERFACE;
    uint64_t call_wire = instruction->operands[
        instruction->opcode == MMAY_OPCODE_CALL_DIRECT_VOID || returns_never ? 0 : 1
    ].unsigned_value;
#ifdef MMAY_HAVE_SLJIT
    const MarimayBytecodeCall *call = prepared != NULL ? prepared->call : call_wire < body->calls.length
#else
    const MarimayBytecodeCall *call = call_wire < body->calls.length
#endif
        ? mmay_bytecode_call(body, (MarimayBytecodeCallId)call_wire)
        : NULL;
    bool returns_void = call != NULL && call->result_type == MMAY_BYTECODE_TYPE_VOID;
    MarimaySymbolId callee_symbol;
    MarimayManagedReference interface_payload =
        mmay_managed_invalid_reference();
#ifdef MMAY_HAVE_SLJIT
    const MarimayBytecodeProgramFunction *callee = prepared != NULL ? prepared->callee : NULL;
#else
    const MarimayBytecodeProgramFunction *callee = NULL;
#endif
    size_t argument_index = 0;
    memset(&callee_symbol, 0, sizeof(callee_symbol));
    if (interface_call && call != NULL) {
        status = mmay_bytecode_run_interface_resolve(
            instruction,
            frame->registers,
            options->execution,
            body,
            &callee_symbol,
            &interface_payload
        );
        if (status != MMAY_BYTECODE_RUN_OK) {
            return status;
        }
    } else if (call != NULL) {
        callee_symbol = call->callee;
    }
#ifdef MMAY_HAVE_SLJIT
    if (prepared != NULL) {
        /* Preparation only projects immutable direct-call metadata. Dynamic
         * depth, storage, roots and publication still pass through this owner. */
        if (prepared->caller != frame->function || prepared->layout == NULL
            || prepared->layout->function != callee || interface_call)
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    } else if (!interface_call && call != NULL && image != NULL) {
#else
    if (!interface_call && call != NULL && image != NULL) {
#endif
        const MarimayBytecodeProgramFunction *functions =
            program->functions.data;
        size_t function_id = (size_t)(frame->function - functions);
        const MarimayBytecodeImageOperation *operation =
            function_id < image->function_count
                ? mmay_bytecode_image_operation_at(
                    &image->functions[function_id], frame->block,
                    frame->instruction) : NULL;
        callee = operation != NULL
            && operation->third < image->function_count
            ? mmay_bytecode_program_function(
                program, operation->third) : NULL;
    } else {
        callee = call != NULL
            ? mmay_bytecode_program_function_by_symbol(
                program, callee_symbol) : NULL;
    }
    if (call == NULL || callee == NULL || count >= options->max_call_depth
    ) {
        status = count >= options->max_call_depth
            ? MMAY_BYTECODE_RUN_STEP_LIMIT
            : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        mmay_bytecode_run_set_trap(*options, instruction->span);
        return status;
    }
    if (!mmay_bytecode_run_frames_reserve(
            &stack->frames,
            &stack->capacity,
            count + 1,
            options->max_call_depth,
            allocator
        )) {
        status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        return status;
    }
    /* Reserve is the only operation that can move the frame vector. Borrow
     * stable slots after it, and publish the new count only once fully rooted. */
    frame = &stack->frames[count - 1];
    MarimayBytecodeRunFrame *next = &stack->frames[count];
    frame->instruction += 1;
#ifdef MMAY_HAVE_SLJIT
    status = prepared != NULL ? mmay_bytecode_run_frame_init_layout(next,
        prepared->layout, allocator, returns_void || returns_never ? 0
            : (uint32_t)instruction->operands[0].unsigned_value,
        returns_void, returns_never, instruction->span) : mmay_bytecode_run_frame_init(
#else
    status = mmay_bytecode_run_frame_init(
#endif
        next,
        callee,
        NULL,
        0,
        allocator,
        returns_void || returns_never
            ? 0
            : (uint32_t)instruction->operands[0].unsigned_value,
        returns_void,
        returns_never,
        instruction->span
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        mmay_bytecode_run_frame_dispose(
            next, &allocator, options->execution
        );
        return status;
    }
    const MarimayBytecodeCallArgument *arguments = body->call_arguments.data;
    const size_t argument_start = call->argument_start;
    const size_t argument_count = call->argument_count;
    /* Verified call arguments occupy one immutable contiguous span. Check it
     * once; reserve/init cannot mutate program metadata. */
    if (argument_count != 0 && (arguments == NULL
        || argument_start > body->call_arguments.length
        || argument_count > body->call_arguments.length - argument_start)) {
        mmay_bytecode_run_frame_dispose(next, &allocator, options->execution);
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    for (argument_index = 0; argument_index < argument_count; argument_index += 1) {
        const MarimayBytecodeCallArgument *argument =
            &arguments[argument_start + argument_index];
        if (interface_call && argument_index == 0) {
            const MarimayBytecodeType *parameter = mmay_vector_at_const(
                &callee->body.register_types,
                0
            );
            if (parameter == NULL
                || !mmay_bytecode_type_is_managed(*parameter)) {
                mmay_bytecode_run_frame_dispose(
                    next,
                    &allocator,
                    options->execution
                );
                status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                return status;
            }
            next->registers[0].type = *parameter;
            next->registers[0].as.managed_reference =
                interface_payload;
        } else {
            next->registers[argument_index] =
                frame->registers[argument->register_id];
        }
    }
    {
#ifdef MMAY_HAVE_SLJIT
        size_t callee_id = prepared != NULL ? 0 : (size_t)(callee
#else
        size_t callee_id = (size_t)(callee
#endif
            - (const MarimayBytecodeProgramFunction *)
                program->functions.data);
#ifdef MMAY_HAVE_SLJIT
        bool borrow_roots = prepared != NULL ? prepared->borrows_caller_roots : image != NULL
#else
        bool borrow_roots = image != NULL
#endif
            && callee_id < image->function_count
            && image->functions[callee_id].borrows_caller_roots;
        if (borrow_roots) {
            mmay_bytecode_run_frame_borrow_caller_roots(
                next);
            status = MMAY_BYTECODE_RUN_OK;
        } else {
#ifdef MMAY_HAVE_SLJIT
            status = prepared != NULL ? mmay_bytecode_run_frame_roots_init_layout(
                next, prepared->layout, options->execution, allocator)
                : mmay_bytecode_run_frame_roots_init(next, options->execution, allocator);
#else
            status = mmay_bytecode_run_frame_roots_init(
                next, options->execution, allocator);
#endif
        }
    }
    if (status != MMAY_BYTECODE_RUN_OK) {
        mmay_bytecode_run_frame_dispose(
            next, &allocator, options->execution
        );
        return status;
    }
    stack->count = count + 1;
    return MMAY_BYTECODE_RUN_OK;
}

#ifdef MMAY_HAVE_SLJIT
MarimayBytecodeRunStatus mmay_bytecode_run_call(
    MarimayBytecodeRunStack *stack, const MarimayBytecodeProgram *program,
    const MarimayBytecodeExecutionImage *image,
    const MarimayBytecodeInstruction *instruction, MarimayAllocator allocator,
    const MarimayBytecodeRunOptions *options) {
    return call_transition(stack, program, image, instruction, allocator, options, NULL);
}

MarimayBytecodeRunStatus mmay_bytecode_run_call_prepared(
    MarimayBytecodeRunStack *stack, const MarimayBytecodePreparedCall *prepared,
    const MarimayBytecodeInstruction *instruction, MarimayAllocator allocator,
    const MarimayBytecodeRunOptions *options) {
    if (prepared == NULL) return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    return call_transition(stack, NULL, NULL, instruction, allocator, options, prepared);
}
#endif

MarimayBytecodeRunStatus mmay_bytecode_run_return(
    MarimayBytecodeRunStack *stack, const MarimayBytecodeInstruction *instruction,
    const MarimayBytecodeRunOptions *options, MarimayBytecodeValue *result,
    bool *finished
) {
    const size_t count = stack->count;
    MarimayBytecodeRunFrame *frame = &stack->frames[count - 1];
    const MarimayBytecodeFunction *body = &frame->function->body;
    MarimayBytecodeRunStatus status = MMAY_BYTECODE_RUN_OK;
    *finished = false;
    MarimayBytecodeValue completed;
    bool returns_void = instruction->opcode == MMAY_OPCODE_RETURN_VOID;
    memset(&completed, 0, sizeof(completed));
    if (returns_void) {
        completed.type = MMAY_BYTECODE_TYPE_VOID;
    } else {
        completed = frame->registers[
            (size_t)instruction->operands[0].unsigned_value
        ];
    }
    if (frame->return_is_never) {
        status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        return status;
    }
    if (completed.type == MMAY_BYTECODE_TYPE_STRING
        && !mmay_bytecode_run_string_value_valid(
            options->execution, completed
        )) {
        status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        return status;
    }
    if (mmay_bytecode_type_is_array(completed.type)
        && !mmay_bytecode_managed_value_valid_for(
            options->execution, body, completed, body->result_layout
        )) {
        status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        return status;
    }
    if (completed.type == MMAY_BYTECODE_TYPE_INTERFACE
        && !mmay_bytecode_run_interface_value_valid(
            options->execution,
            body,
            completed,
            body->result_layout
        )) {
        status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        return status;
    }
    if (count == 1) {
        *result = completed;
        *finished = true;
        status = MMAY_BYTECODE_RUN_OK;
        return status;
    }
    {
        MarimayBytecodeRunFrame *parent = &stack->frames[count - 2];
        uint32_t destination = frame->return_destination;
        bool return_is_void = frame->return_is_void;
        if (!return_is_void) {
            parent->registers[destination] = completed;
        }
        mmay_bytecode_run_frame_release(frame, options->execution);
        stack->count = count - 1;
    }
    return MMAY_BYTECODE_RUN_OK;
}
