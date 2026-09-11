#include "vm_activation.h"
#include "vm_internal.h"
#include <string.h>

/* One publication/root protocol serves resolved ordinary and prepared calls.
 * Resolution projects immutable metadata; only this owner activates a callee. */
static MarimayBytecodeRunStatus activate_call(
    MarimayBytecodeRunStack *stack, const MarimayBytecodePreparedCall *binding,
    const MarimayBytecodeInstruction *instruction, MarimayAllocator allocator,
    const MarimayBytecodeRunOptions *options,
    const MarimayManagedReference *interface_payload) {
    const size_t count = stack->count;
    const MarimayBytecodeCall *call = binding->call;
    const MarimayBytecodeProgramFunction *callee = binding->callee;
    if (call == NULL || callee == NULL || count >= options->max_call_depth) {
        mmay_bytecode_run_set_trap(*options, instruction->span);
        return count >= options->max_call_depth ? MMAY_BYTECODE_RUN_STEP_LIMIT
            : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (count >= stack->capacity && !mmay_bytecode_run_frames_reserve(
            &stack->frames, &stack->capacity, count + 1,
            options->max_call_depth, allocator))
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    /* Reserve alone can move the frame vector. Publish count only after roots. */
    MarimayBytecodeRunFrame *frame = &stack->frames[count - 1];
    MarimayBytecodeRunFrame *next = &stack->frames[count];
    const MarimayBytecodeFunction *body = &frame->function->body;
    bool returns_void = call->result_type == MMAY_BYTECODE_TYPE_VOID;
    bool returns_never = instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER;
    uint32_t destination = returns_void || returns_never ? 0
        : (uint32_t)instruction->operands[0].unsigned_value;
    frame->instruction += 1;
#ifdef MMAY_HAVE_SLJIT
    MarimayBytecodeRunStatus status = binding->layout != NULL
        ? mmay_bytecode_run_frame_init_layout(next, binding->layout, allocator,
            destination, returns_void, returns_never, instruction->span)
        : mmay_bytecode_run_frame_init(next, callee, NULL, 0, allocator,
            destination, returns_void, returns_never, instruction->span);
#else
    MarimayBytecodeRunStatus status = mmay_bytecode_run_frame_init(next, callee,
        NULL, 0, allocator, destination, returns_void, returns_never, instruction->span);
#endif
    if (status != MMAY_BYTECODE_RUN_OK) goto failed;
    const size_t argument_count = call->argument_count;
    const MarimayBytecodeCallArgument *arguments = binding->arguments;
#ifdef MMAY_HAVE_SLJIT
    if (binding->layout == NULL) {
#endif
        const size_t start = call->argument_start;
        if (argument_count != 0 && (body->call_arguments.data == NULL
            || start > body->call_arguments.length
            || argument_count > body->call_arguments.length - start)) {
            status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            goto failed;
        }
        if (argument_count != 0)
            arguments = (const MarimayBytecodeCallArgument *)body->call_arguments.data + start;
#ifdef MMAY_HAVE_SLJIT
    }
#endif
    for (size_t index = 0; index < argument_count; ++index) {
        if (interface_payload != NULL && index == 0) {
            const MarimayBytecodeType *parameter = mmay_vector_at_const(
                &callee->body.register_types, 0);
            if (parameter == NULL || !mmay_bytecode_type_is_managed(*parameter)) {
                status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                goto failed;
            }
            next->registers[0].type = *parameter;
            next->registers[0].as.managed_reference = *interface_payload;
        } else next->registers[index] = frame->registers[arguments[index].register_id];
    }
    if (binding->borrows_caller_roots) {
        mmay_bytecode_run_frame_borrow_caller_roots(next);
    } else {
#ifdef MMAY_HAVE_SLJIT
        status = binding->layout != NULL ? mmay_bytecode_run_frame_roots_init_layout(
            next, binding->layout, options->execution, allocator)
            : mmay_bytecode_run_frame_roots_init(next, options->execution, allocator);
#else
        status = mmay_bytecode_run_frame_roots_init(next, options->execution, allocator);
#endif
        if (status != MMAY_BYTECODE_RUN_OK) goto failed;
    }
    stack->count = count + 1;
    return MMAY_BYTECODE_RUN_OK;
failed:
    mmay_bytecode_run_frame_dispose(next, &allocator, options->execution);
    return status;
}

MarimayBytecodeRunStatus mmay_bytecode_run_call(
    MarimayBytecodeRunStack *stack, const MarimayBytecodeProgram *program,
    const MarimayBytecodeExecutionImage *image,
    const MarimayBytecodeInstruction *instruction, MarimayAllocator allocator,
    const MarimayBytecodeRunOptions *options) {
    MarimayBytecodeRunFrame *frame = &stack->frames[stack->count - 1];
    const MarimayBytecodeFunction *body = &frame->function->body;
    bool interface_call = instruction->opcode == MMAY_OPCODE_CALL_INTERFACE;
    uint64_t wire = instruction->operands[
        instruction->opcode == MMAY_OPCODE_CALL_DIRECT_VOID
        || instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER ? 0 : 1].unsigned_value;
    const MarimayBytecodeCall *call = wire < body->calls.length
        ? mmay_bytecode_call(body, (MarimayBytecodeCallId)wire) : NULL;
    MarimaySymbolId symbol = {{0}};
    MarimayManagedReference payload = mmay_managed_invalid_reference();
    if (interface_call && call != NULL) {
        MarimayBytecodeRunStatus status = mmay_bytecode_run_interface_resolve(
            instruction, frame->registers, options->execution, body, &symbol, &payload);
        if (status != MMAY_BYTECODE_RUN_OK) return status;
    } else if (call != NULL) symbol = call->callee;
    const MarimayBytecodeProgramFunction *callee = NULL;
    if (!interface_call && call != NULL && image != NULL) {
        const MarimayBytecodeProgramFunction *functions = program->functions.data;
        size_t id = (size_t)(frame->function - functions);
        const MarimayBytecodeImageOperation *op = id < image->function_count
            ? mmay_bytecode_image_operation_at(&image->functions[id], frame->block,
                frame->instruction) : NULL;
        callee = op != NULL && op->third < image->function_count
            ? mmay_bytecode_program_function(program, op->third) : NULL;
    } else if (call != NULL) callee = mmay_bytecode_program_function_by_symbol(program, symbol);
    bool borrow = false;
    if (callee != NULL && image != NULL) {
        size_t id = (size_t)(callee - (const MarimayBytecodeProgramFunction *)program->functions.data);
        borrow = id < image->function_count && image->functions[id].borrows_caller_roots;
    }
    MarimayBytecodePreparedCall binding = {.caller=frame->function, .callee=callee,
        .call=call, .borrows_caller_roots=borrow};
    return activate_call(stack, &binding, instruction, allocator, options,
        interface_call ? &payload : NULL);
}

#ifdef MMAY_HAVE_SLJIT
MarimayBytecodeRunStatus mmay_bytecode_run_call_prepared(
    MarimayBytecodeRunStack *stack, const MarimayBytecodePreparedCall *prepared,
    const MarimayBytecodeInstruction *instruction, MarimayAllocator allocator,
    const MarimayBytecodeRunOptions *options) {
    if (prepared == NULL || prepared->caller != stack->frames[stack->count - 1].function
        || prepared->layout == NULL || prepared->layout->function != prepared->callee
        || instruction->opcode == MMAY_OPCODE_CALL_INTERFACE)
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    return activate_call(stack, prepared, instruction, allocator, options, NULL);
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
