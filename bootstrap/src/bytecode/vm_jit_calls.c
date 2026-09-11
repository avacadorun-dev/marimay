#include "vm_jit_internal.h"
#include <string.h>

/* Every generated function uses the same SLJIT register/stack context. A label
 * transfer changes the logical activation without growing the native stack. */
static void *resume(MarimayBytecodeJitContext *context, size_t id, size_t pc) {
    MarimayBytecodeJitActivation *activation = context->activation;
    MarimayBytecodeJit *jit = activation->jit;
    MarimayBytecodeRunFrame *frame = &activation->stack->frames[activation->stack->count - 1];
    context->function = &frame->function->body;
    context->image = &activation->image->functions[id];
    context->registers = frame->registers;
    context->pc = pc;
    jit->epoch += 1;
    if (jit->epoch == 0) {
        mmay_bytecode_jit_clear_bindings(jit);
        jit->epoch = 1;
    }
    context->epoch = jit->epoch;
    if (id >= jit->function_count) return NULL;
    const MarimayBytecodeJitFunction *compiled = &jit->functions[id];
    context->bindings = compiled->bindings != NULL ? compiled->bindings : jit->bindings;
    if (compiled->entries == NULL || compiled->code == NULL
        || context->pc >= context->image->operation_count) return NULL;
    return (unsigned char *)compiled->code + (compiled->entries[context->pc] & MMAY_JIT_OFFSET);
}

/* Source publication precedes all guards; a miss remains at the original call. */
static MarimayBytecodeJitActivation *publish(MarimayBytecodeJitContext *context, uint32_t pc) {
    context->pc = pc;
    MarimayBytecodeJitActivation *activation = context->activation;
    if (activation == NULL) return NULL;
    const MarimayBytecodeImageOperation *op = &context->image->operations[pc];
    MarimayBytecodeRunFrame *frame = &activation->stack->frames[activation->stack->count - 1];
    frame->block = op->source_block;
    frame->instruction = op->source_instruction;
    return activation;
}

void *mmay_bytecode_jit_call(MarimayBytecodeJitContext *context, uint32_t pc,
    const MarimayBytecodeInstruction *instruction, const MarimayBytecodePreparedCall *call) {
    MarimayBytecodeJitActivation *activation = publish(context, pc);
    if (activation == NULL || call == NULL || call->callee == NULL
        || call->callee_id >= activation->jit->function_count
        || activation->jit->functions[call->callee_id].code == NULL) return NULL;
    activation->status = mmay_bytecode_run_call_prepared(activation->stack,
        call, instruction, activation->allocator, activation->options);
    if (activation->status != MMAY_BYTECODE_RUN_OK) return NULL;
    /* The shared owner starts every callee at block/instruction zero. */
    return resume(context, call->callee_id, 0);
}

void *mmay_bytecode_jit_return(MarimayBytecodeJitContext *context, uint32_t pc,
    const MarimayBytecodeInstruction *instruction) {
    MarimayBytecodeJitActivation *activation = publish(context, pc);
    if (activation == NULL) return NULL;
    activation->status = mmay_bytecode_run_return(activation->stack,
        instruction, activation->options, activation->result, &activation->finished);
    if (activation->status != MMAY_BYTECODE_RUN_OK || activation->finished) return NULL;
    const MarimayBytecodeRunFrame *frame = &activation->stack->frames[activation->stack->count - 1];
    size_t id = (size_t)(frame->function
        - (const MarimayBytecodeProgramFunction *)activation->program->functions.data);
    size_t next = (size_t)activation->image->functions[id].block_starts[frame->block]
        + frame->instruction;
    return resume(context, id, next);
}

void mmay_bytecode_jit_start(MarimayBytecodeJit *jit,
    const MarimayBytecodeProgram *program, const MarimayBytecodeExecutionImage *image,
    MarimayBytecodeRunStack *stack, MarimayAllocator allocator,
    const MarimayBytecodeRunOptions *options, MarimayBytecodeValue *result
) {
    if (!jit->ready) return;
    jit->state->activation = (MarimayBytecodeJitActivation){
        jit, program, image, stack, allocator, options, result, MMAY_BYTECODE_RUN_OK, false
    };
    jit->state->context = (MarimayBytecodeJitContext){
        .bindings=jit->bindings, .execution=options->execution,
        .budget=SIZE_MAX, .activation=&jit->state->activation
    };
}

bool mmay_bytecode_jit_run_stack(MarimayBytecodeJit *jit,
    MarimayBytecodeRunStatus *status, bool *finished
) {
    *finished = false;
    *status = MMAY_BYTECODE_RUN_OK;
    if (!jit->ready || jit->state->activation.jit == NULL) return false;
    MarimayBytecodeJitActivation *activation = &jit->state->activation;
    MarimayBytecodeJitContext *context = &jit->state->context;
    MarimayBytecodeRunStack *stack = activation->stack;
    const MarimayBytecodeRunOptions *options = activation->options;
    MarimayBytecodeRunFrame *frame = &stack->frames[stack->count - 1];
    size_t id = (size_t)(frame->function
        - (const MarimayBytecodeProgramFunction *)activation->program->functions.data);
    const MarimayBytecodeImageFunction *image = &activation->image->functions[id];
    if (jit->meters_fuel) {
        return mmay_bytecode_jit_run(jit, id, image,
            &frame->function->body, frame->registers, options->execution,
            &frame->block, &frame->instruction, &frame->steps, *options);
    }
    if (options->max_steps != SIZE_MAX || options->remaining_shared_steps != NULL) return false;
    if (id >= jit->function_count) return false;
    const MarimayBytecodeJitFunction *compiled = &jit->functions[id];
    if (compiled->entries == NULL || compiled->code == NULL) return false;
    size_t pc = (size_t)image->block_starts[frame->block] + frame->instruction;
    if (pc >= image->operation_count) return false;
    /* A cold instruction needs neither a native activation nor new bindings. */
    if (!(compiled->entries[pc] & MMAY_JIT_ENTRY)) return true;
    context->function = &frame->function->body;
    context->image = image;
    context->registers = frame->registers;
    context->bindings = compiled->bindings != NULL ? compiled->bindings : jit->bindings;
    context->pc = pc;
    jit->epoch += 1;
    if (jit->epoch == 0) {
        mmay_bytecode_jit_clear_bindings(jit);
        jit->epoch = 1;
    }
    context->epoch = jit->epoch;
    mmay_bytecode_jit_enter(compiled, context, pc);
    *finished = activation->finished;
    *status = activation->status;
    if (!*finished && *status == MMAY_BYTECODE_RUN_OK) {
        frame = &stack->frames[stack->count - 1];
        frame->block = context->image->operations[context->pc].source_block;
        frame->instruction = context->image->operations[context->pc].source_instruction;
    }
    return true;
}
