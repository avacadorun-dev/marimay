#include "vm_jit_internal.h"

#include <stdalign.h>
#include <string.h>

void mmay_bytecode_jit_dispose(MarimayBytecodeJit *jit) {
    if (jit == NULL) return;
    for (size_t i = 0; i < jit->function_count; ++i)
        mmay_code_memory_dispose(&jit->functions[i].memory);
    mmay_deallocate(&jit->allocator, jit->bindings, jit->binding_bytes);
    mmay_deallocate(&jit->allocator, jit->metadata, jit->metadata_bytes);
    memset(jit, 0, sizeof(*jit));
}

MarimayBytecodeRunStatus mmay_bytecode_jit_init(MarimayBytecodeJit *jit,
    const MarimayBytecodeProgram *program,
    const MarimayBytecodeExecutionImage *image, MarimayAllocator allocator,
    MarimayCodeMemoryFault *fault, bool meter_fuel
) {
    memset(jit, 0, sizeof(*jit));
    jit->allocator = allocator;
    jit->meters_fuel = meter_fuel;
    if (!mmay_code_memory_supported()) return MMAY_BYTECODE_RUN_OK;

    void *selection = NULL;
    size_t selection_bytes = 0;
    unsigned char *selected = NULL;
    MarimayBytecodeRunStatus selection_status = mmay_bytecode_jit_select(
        program, image, allocator, meter_fuel, &selection, &selection_bytes, &selected);
    if (selection_status != MMAY_BYTECODE_RUN_OK) return selection_status;
    if (selected == NULL) return MMAY_BYTECODE_RUN_OK;

    size_t bytes = 0, registers = 0;
    if (!mmay_size_multiply(image->function_count,
            sizeof(MarimayBytecodeJitFunction), &bytes)) goto failed;
    _Static_assert(sizeof(MarimayBytecodeJitState) % alignof(MarimayBytecodeJitFunction) == 0,
        "state prefix preserves function metadata alignment");
    if (!mmay_size_add(bytes, sizeof(MarimayBytecodeJitState), &bytes)) goto failed;
    size_t entries_start = bytes;
    const MarimayBytecodeProgramFunction *functions = program->functions.data;
    for (size_t index = 0; index < image->function_count; index += 1) {
        if (!selected[index]) continue;
        size_t entries = 0;
        if (!mmay_size_multiply(image->functions[index].operation_count,
                sizeof(uint32_t) + sizeof(MarimayBytecodeJitPlanOperation), &entries)
            || !mmay_size_add(bytes, entries, &bytes)) goto failed;
        if (functions[index].body.register_types.length > registers) {
            registers = functions[index].body.register_types.length;
        }
    }
    if (registers == 0) {
        mmay_deallocate(&allocator, selection, selection_bytes);
        return MMAY_BYTECODE_RUN_OK;
    }
    jit->metadata = mmay_allocate(&allocator, bytes);
    if (jit->metadata == NULL) goto failed;
    jit->metadata_bytes = bytes;
    memset(jit->metadata, 0, bytes);
    jit->state = jit->metadata;
    jit->functions = (MarimayBytecodeJitFunction *)((unsigned char *)jit->metadata
        + sizeof(MarimayBytecodeJitState));
    jit->function_count = image->function_count;
    size_t position = entries_start, code_bytes = 0;
    for (size_t index = 0; index < image->function_count; index += 1) {
        const MarimayBytecodeFunction *function = &functions[index].body;
        const MarimayBytecodeImageFunction *body = &image->functions[index];
        if (!selected[index]) continue;
        MarimayBytecodeJitFunction *compiled = &jit->functions[index];
        compiled->entries = (uint32_t *)((unsigned char *)jit->metadata + position);
        position += body->operation_count * sizeof(uint32_t);
        compiled->plan = (MarimayBytecodeJitPlanOperation *)
            ((unsigned char *)jit->metadata + position);
        position += body->operation_count * sizeof(*compiled->plan);
        mmay_bytecode_jit_plan(body, function, compiled->plan, meter_fuel);
        MarimayCodeMemoryStatus compiled_status = mmay_bytecode_jit_sljit(body, function, compiled, &allocator,
            MMAY_JIT_MAX_CODE_BYTES - code_bytes, fault, meter_fuel);
        if (compiled_status == MMAY_CODE_MEMORY_ALLOCATION_FAILED) goto failed;
        if (compiled_status == MMAY_CODE_MEMORY_LIMIT) { compiled->entries = NULL; continue; }
        if (compiled_status != MMAY_CODE_MEMORY_OK) {
            mmay_deallocate(&allocator, selection, selection_bytes);
            mmay_bytecode_jit_dispose(jit);
            return compiled_status == MMAY_CODE_MEMORY_UNAVAILABLE ? MMAY_BYTECODE_RUN_OK : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        code_bytes += compiled->memory.bytes;
    }
    jit->ready = code_bytes != 0;
    if (!mmay_size_multiply(registers, sizeof(MarimayBytecodeJitBinding),
            &jit->binding_bytes)) goto failed;
    jit->bindings = mmay_allocate(&allocator, jit->binding_bytes);
    if (jit->bindings == NULL) goto failed;
    memset(jit->bindings, 0, jit->binding_bytes);
    mmay_deallocate(&allocator, selection, selection_bytes);
    return MMAY_BYTECODE_RUN_OK;
failed:
    mmay_deallocate(&allocator, selection, selection_bytes);
    mmay_bytecode_jit_dispose(jit);
    return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
}

bool mmay_bytecode_jit_run(MarimayBytecodeJit *jit, size_t function_id,
    const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, size_t *block, size_t *instruction,
    size_t *steps, MarimayBytecodeRunOptions options
) {
    if (!jit->ready || function_id >= jit->function_count) return false;
    if (!jit->meters_fuel && (options.max_steps != SIZE_MAX
        || options.remaining_shared_steps != NULL)) return false;
    const MarimayBytecodeJitFunction *compiled = &jit->functions[function_id];
    if (compiled->entries == NULL) return false;
    size_t pc = (size_t)image->block_starts[*block] + *instruction;
    if (pc >= image->operation_count) return false;
    if (!(compiled->entries[pc] & MMAY_JIT_ENTRY)) return true;
    size_t budget = options.max_steps == SIZE_MAX ? SIZE_MAX
        : *steps < options.max_steps ? options.max_steps - *steps : 0;
    if (options.remaining_shared_steps != NULL
        && budget > *options.remaining_shared_steps) budget = *options.remaining_shared_steps;
    jit->epoch += 1;
    if (jit->epoch == 0) {
        memset(jit->bindings, 0, jit->binding_bytes);
        jit->epoch = 1;
    }
    MarimayBytecodeJitContext context = {
        registers, jit->bindings, function, image, execution, jit->epoch, budget, pc, NULL
    };
    mmay_bytecode_jit_enter(compiled, &context, pc);
    size_t consumed = budget - context.budget;
    if (options.max_steps != SIZE_MAX) *steps += consumed;
    if (options.remaining_shared_steps != NULL) *options.remaining_shared_steps -= consumed;
    *block = image->operations[context.pc].source_block;
    *instruction = image->operations[context.pc].source_instruction;
    return true;
}

void mmay_bytecode_jit_enter(const MarimayBytecodeJitFunction *compiled,
    MarimayBytecodeJitContext *context, size_t pc) {
    const unsigned char *base = (const unsigned char *)compiled->code;
    typedef void (*MarimayJitEntry)(MarimayBytecodeJitContext *, const void *);
    MarimayJitEntry entry = NULL;
    _Static_assert(sizeof(entry) == sizeof(base), "host code-pointer representation");
    memcpy(&entry, &base, sizeof(entry));
    entry(context, base + (compiled->entries[pc] & MMAY_JIT_OFFSET));
}
