#include "vm_jit_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "JIT view proof: %s at %d\n", #c, __LINE__); exit(1); } } while (0)

static void append(MarimayBytecodeFunction *f, uint32_t block,
    MarimayOpcode opcode, uint32_t a, uint32_t b, uint32_t c) {
    MarimayBytecodeInstruction op = {.opcode = opcode};
    op.operands[0].unsigned_value = a; op.operands[1].unsigned_value = b;
    op.operands[2].unsigned_value = c;
    EXPECT(mmay_bytecode_append_instruction(f, block, &op));
}

static void fixture(MarimayBytecodeProgram *program) {
    MarimayAllocator a = mmay_system_allocator();
    EXPECT(mmay_bytecode_program_init(program, a, 1, mmay_bytecode_program_default_options()));
    MarimayBytecodeFunction f = {0};
    MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_ARRAY_F64;
    EXPECT(mmay_bytecode_function_init(&f, a, MMAY_BYTECODE_TYPE_I64, 1,
        &parameter, 1, mmay_bytecode_function_default_options()));
    uint32_t reg, block, id;
    EXPECT(mmay_bytecode_add_register(&f, MMAY_BYTECODE_TYPE_I64, &reg));
    EXPECT(mmay_bytecode_add_register(&f, MMAY_BYTECODE_TYPE_BOOL, &reg));
    EXPECT(mmay_bytecode_add_block(&f, &block));
    EXPECT(mmay_bytecode_add_block(&f, &block));
    append(&f, 0, MMAY_OPCODE_CONST_BOOL, 2, 0, 0);
    append(&f, 0, MMAY_OPCODE_BRANCH_BOOL, 2, 0, 1);
    append(&f, 1, MMAY_OPCODE_ARRAY_LENGTH, 1, 0, 0);
    append(&f, 1, MMAY_OPCODE_RETURN, 1, 0, 0);
    MarimaySymbolId symbol = {{17}};
    EXPECT(mmay_bytecode_program_take_function(program, symbol, &f, &id));
    EXPECT(mmay_bytecode_program_set_entry(program, symbol));
    EXPECT(mmay_bytecode_program_verify(program) == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static void native_proofs(void) {
    MarimayBytecodeProgram program = {0};
    fixture(&program);
    MarimayFailAllocator ledger;
    mmay_fail_allocator_init(&ledger, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator a = mmay_fail_allocator(&ledger);
    MarimayBytecodeExecution execution = {0};
    EXPECT(mmay_bytecode_execution_init(&execution, a, mmay_bytecode_execution_default_options()));
    MarimayManagedCell cells[2] = {{.f64_bits = 1}, {.f64_bits = 2}};
    MarimayManagedReference reference;
    EXPECT(mmay_managed_array_new(&execution.managed_heap, &mmay_managed_array_f64_descriptor,
        cells, 2, &reference) == MMAY_MANAGED_OK);
    MarimayBytecodeExecutionImage image = {0};
    MarimayBytecodeJit jit = {0};
    MarimayCodeMemoryFault fault = {SIZE_MAX, 0, 0, 0};
    EXPECT(mmay_bytecode_execution_image_init(&image, &program, a) == MMAY_BYTECODE_IMAGE_OK);
    EXPECT(mmay_bytecode_jit_init(&jit, &program, &image, a, &fault, false) == MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready && jit.functions[0].bindings != NULL);
    MarimayBytecodeValue registers[3] = {
        {.type = MMAY_BYTECODE_TYPE_ARRAY_F64, .as.managed_reference = reference},
        {.type = MMAY_BYTECODE_TYPE_I64}, {.type = MMAY_BYTECODE_TYPE_BOOL}
    };
    const MarimayBytecodeFunction *body = &mmay_bytecode_program_function(&program, 0)->body;
    MarimayBytecodeJitBinding bindings[3] = {0};
    MarimayBytecodeJitContext context = {.registers = registers, .bindings = bindings,
        .function = body, .image = &image.functions[0], .execution = &execution,
        .budget = SIZE_MAX, .epoch = 1};
    EXPECT(mmay_bytecode_jit_bind(&context, 0) != NULL);
    MarimayBytecodeJitBinding valid = bindings[0];
    MarimayManagedHeap saved_heap = execution.managed_heap;
    MarimayBytecodeFunction alternate = *body;
    for (size_t kind = 0; kind < 18; ++kind) {
        execution.managed_heap = saved_heap;
        execution.initialized = true;
        registers[0].type = MMAY_BYTECODE_TYPE_ARRAY_F64;
        registers[0].as.managed_reference = reference;
        bindings[0] = valid;
        context.epoch = 2 + kind;
        context.function = body;
        context.execution = &execution;
        size_t poison_length = 991;
        /* A missing revalidation clause would read this deliberately wrong
         * retained pointer. The checked oracle must replace or reject it. */
        if (kind != 0) bindings[0].view.array.length = &poison_length;
        switch (kind) {
            case 1: bindings[0].function = NULL; break;
            case 2: bindings[0].layout ^= 1; break;
            case 3: bindings[0].type = MMAY_BYTECODE_TYPE_BOOL; break;
            case 4: bindings[0].execution = NULL; break;
            case 5: bindings[0].view_epoch = 0; break;
            case 6: bindings[0].view.array.reference.heap_provenance ^= 1; break;
            case 7: bindings[0].view.array.reference.slot ^= 1; break;
            case 8: bindings[0].view.array.reference.generation ^= 1; break;
            case 9:
                execution.managed_heap.view_epoch = UINT64_MAX;
                bindings[0].view_epoch = UINT64_MAX; break;
            case 10: execution.initialized = false; break;
            case 11: execution.managed_heap.initialized = false; break;
            case 12: execution.managed_heap.collecting = true; break;
            case 13: execution.managed_heap.provenance ^= 1; break;
            case 14: registers[0].as.managed_reference.generation ^= 1; break;
            case 15: context.function = &alternate; break;
            case 16: context.execution = NULL; break;
            case 17: registers[0].type = MMAY_BYTECODE_TYPE_BOOL; break;
            default: break;
        }
        MarimayBytecodeJitBinding oracle_bindings[3] = {bindings[0]};
        MarimayBytecodeJitContext oracle = context;
        oracle.bindings = oracle_bindings;
        bool expected = mmay_bytecode_jit_bind(&oracle, 0) != NULL;
        size_t expected_length = expected ? *oracle_bindings[0].view.array.length : 0;
        registers[1].as.i64 = -123;
        context.pc = 2;
        size_t attempts = ledger.attempts;
        mmay_bytecode_jit_enter(&jit.functions[0], &context, 2);
        EXPECT(ledger.attempts == attempts);
        EXPECT(context.pc == (expected ? 3 : 2));
        EXPECT(registers[1].as.i64 == (expected ? (int64_t)expected_length : -123));
        EXPECT(!expected || expected_length == 2);
        EXPECT(bindings[0].epoch == oracle_bindings[0].epoch);
    }
    execution.managed_heap = saved_heap;
    execution.initialized = true;
    mmay_bytecode_jit_dispose(&jit);
    EXPECT(fault.live_mappings == 0 && fault.live_bytes == 0);
    mmay_bytecode_execution_image_dispose(&image);
    mmay_bytecode_execution_dispose(&execution);
    EXPECT(ledger.live_allocations == 0 && ledger.live_bytes == 0);
    mmay_bytecode_program_dispose(&program);
}

static void bounded_tables(void) {
    enum { count = 20 };
    MarimayBytecodeProgram program = {0};
    MarimayAllocator system = mmay_system_allocator();
    EXPECT(mmay_bytecode_program_init(&program, system, 1, mmay_bytecode_program_default_options()));
    for (uint32_t id = 0; id < count; ++id) {
        MarimayBytecodeFunction f = {0};
        EXPECT(mmay_bytecode_function_init(&f, system, MMAY_BYTECODE_TYPE_VOID, 1,
            NULL, 0, mmay_bytecode_function_default_options()));
        uint32_t reg, block, added;
        for (size_t i = 0; i < MMAY_JIT_MAX_REGISTERS; ++i)
            EXPECT(mmay_bytecode_add_register(&f, MMAY_BYTECODE_TYPE_I64, &reg));
        EXPECT(mmay_bytecode_add_block(&f, &block));
        append(&f, 0, MMAY_OPCODE_RETURN_VOID, 0, 0, 0);
        MarimaySymbolId symbol = {{(unsigned char)(id + 1)}};
        EXPECT(mmay_bytecode_program_take_function(&program, symbol, &f, &added));
        if (id == 0) EXPECT(mmay_bytecode_program_set_entry(&program, symbol));
    }
    EXPECT(mmay_bytecode_program_verify(&program) == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    MarimayBytecodeExecutionImage image = {0};
    EXPECT(mmay_bytecode_execution_image_init(&image, &program, system) == MMAY_BYTECODE_IMAGE_OK);
    for (size_t failure = 0; failure < 2; ++failure) {
        MarimayFailAllocator ledger;
        mmay_fail_allocator_init(&ledger, system, failure);
        MarimayBytecodeJitFunction functions[count] = {0};
        MarimayBytecodeJitState state = {0};
        MarimayBytecodeJit jit = {.allocator = mmay_fail_allocator(&ledger),
            .functions = functions, .function_count = count, .state = &state};
        unsigned char selected[count];
        memset(selected, 1, sizeof(selected));
        MarimayBytecodeRunStatus status = mmay_bytecode_jit_prepare_activations(
            &jit, &program, &image, selected);
        EXPECT(status == (failure == 0 ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED : MMAY_BYTECODE_RUN_OK));
        if (status == MMAY_BYTECODE_RUN_OK) {
            size_t bytes = 0, uncached = 0;
            for (size_t i = 0; i < count; ++i) {
                bytes += functions[i].binding_bytes;
                uncached += functions[i].bindings == NULL ? 1 : 0;
            }
            EXPECT(bytes <= MMAY_JIT_MAX_VIEW_BYTES && bytes != 0 && uncached != 0);
            EXPECT(ledger.attempts == 1);
        }
        mmay_bytecode_jit_dispose(&jit);
        EXPECT(ledger.live_allocations == 0 && ledger.live_bytes == 0);
    }
    mmay_bytecode_execution_image_dispose(&image);
    mmay_bytecode_program_dispose(&program);
}

int main(void) {
    bounded_tables();
    if (!mmay_code_memory_supported()) return 77;
    native_proofs();
    puts("JIT retained-view predicates: native/checked equivalence, precise misses, bounded tables and zero ledgers: passed");
    return 0;
}
