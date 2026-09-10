#include "vm_jit_internal.h"
#include "vm_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(condition) do { if (!(condition)) { fprintf(stderr, "JIT call roots failed: %s at %d\n", #condition, __LINE__); exit(1); } } while (0)

static void append(MarimayBytecodeFunction *function, uint32_t block,
    MarimayOpcode opcode, uint64_t a, uint64_t b, uint64_t c, uint64_t span) {
    MarimayBytecodeInstruction instruction = {.opcode=opcode,
        .span={.start=span, .end=span + 1}};
    instruction.operands[0].unsigned_value = a;
    instruction.operands[1].unsigned_value = b;
    instruction.operands[2].unsigned_value = c;
    EXPECT(mmay_bytecode_append_instruction(function, block, &instruction));
}

static void fixture(MarimayBytecodeProgram *program) {
    MarimayAllocator allocator = mmay_system_allocator();
    EXPECT(mmay_bytecode_program_init(program, allocator, 1,
        mmay_bytecode_program_default_options()));
    MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_ARRAY_F64;
    const MarimayBytecodeType locals[] = {MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_F64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_ARRAY_F64, MMAY_BYTECODE_TYPE_BOOL};
    for (uint32_t id = 0; id < 3; ++id) {
        MarimayBytecodeFunction function = {0};
        uint32_t added = 0, block = 0;
        EXPECT(mmay_bytecode_function_init(&function, allocator, MMAY_BYTECODE_TYPE_I64,
            1, &parameter, 1, mmay_bytecode_function_default_options()));
        for (size_t i = 0; i < sizeof(locals) / sizeof(*locals); ++i)
            EXPECT(mmay_bytecode_add_register(&function, locals[i], &added));
        EXPECT(mmay_bytecode_add_block(&function, &block));
        if (id == 2) {
            EXPECT(mmay_bytecode_add_block(&function, &block));
            append(&function, 0, MMAY_OPCODE_CONST_BOOL, 5, 0, 0, 28);
            append(&function, 0, MMAY_OPCODE_BRANCH_BOOL, 5, 0, block, 29);
        }
        if (id == 0) {
            MarimayBytecodeArrayConstructionId construction;
            EXPECT(mmay_bytecode_add_array_construction(&function,
                MMAY_BYTECODE_TYPE_ARRAY_F64, NULL, 0, &construction));
            append(&function, block, MMAY_OPCODE_CONST_I64, 1, 0, 0, 50);
            append(&function, block, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 4, 0, 1, 51);
            append(&function, block, MMAY_OPCODE_CONST_F64, 2, UINT64_C(0x4045000000000000), 0, 51);
            append(&function, block, MMAY_OPCODE_ARRAY_SET, 4, 1, 2, 52);
            append(&function, block, MMAY_OPCODE_ARRAY_NEW, 4, construction, 0, 53);
            append(&function, block, MMAY_OPCODE_ARRAY_GET, 2, 0, 1, 54);
            append(&function, block, MMAY_OPCODE_ARRAY_LENGTH, 3, 4, 0, 55);
        } else {
            MarimayBytecodeCallId call;
            MarimaySymbolId callee = {{(unsigned char)(id * 0x10)}};
            MarimayBytecodeCallArgument argument = {.register_id=0,
                .type=parameter, .layout=MMAY_BYTECODE_NO_LAYOUT};
            EXPECT(mmay_bytecode_add_call(&function, callee, MMAY_BYTECODE_TYPE_I64,
                &argument, 1, &call));
            append(&function, block, MMAY_OPCODE_CALL_DIRECT, 3, call, 0, id == 1 ? 40 : 30);
        }
        append(&function, block, MMAY_OPCODE_RETURN, 3, 0, 0, id == 0 ? 56 : id == 1 ? 41 : 31);
        MarimaySymbolId symbol = {{(unsigned char)((id + 1) * 0x10)}};
        EXPECT(mmay_bytecode_program_take_function(program, symbol, &function, &added));
        if (id == 2) EXPECT(mmay_bytecode_program_set_entry(program, symbol));
    }
    EXPECT(mmay_bytecode_program_verify(program) == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static void execution_init(MarimayBytecodeExecution *execution,
    MarimayAllocator allocator, MarimayBytecodeValue *argument, size_t length) {
    MarimayBytecodeExecutionOptions options = mmay_bytecode_execution_default_options();
    options.managed_heap.collect_before_allocation = true;
    EXPECT(mmay_bytecode_execution_init(execution, allocator, options));
    MarimayManagedCell cell = {.f64_bits=0};
    argument->type = MMAY_BYTECODE_TYPE_ARRAY_F64;
    EXPECT(mmay_managed_array_new(&execution->managed_heap,
        &mmay_managed_array_f64_descriptor, &cell, length, &argument->as.managed_reference)
        == MMAY_MANAGED_OK);
}

static void written_once(MarimayBytecodeExecution *execution, MarimayBytecodeValue argument) {
    MarimayManagedArrayView view;
    EXPECT(mmay_managed_array_view_bind(&execution->managed_heap,
        argument.as.managed_reference, &mmay_managed_array_f64_descriptor, &view) == MMAY_MANAGED_OK);
    EXPECT(*view.mutation_generation == 1);
    MarimayManagedCell cell;
    size_t length = 0;
    EXPECT(mmay_managed_array_get(&execution->managed_heap, argument.as.managed_reference,
        0, &cell, &length) == MMAY_MANAGED_OK);
    EXPECT(length == 1 && cell.f64_bits == UINT64_C(0x4045000000000000));
}

static void native_exit(const MarimayBytecodeProgram *program) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&failing);
    MarimayCodeMemoryFault fault = {SIZE_MAX, 0, 0, 0};
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeValue argument = {0};
    execution_init(&execution, allocator, &argument, 1);
    MarimayBytecodeExecutionImage image = {0};
    MarimayBytecodeJit jit = {0};
    MarimayBytecodeRunStack stack = {0};
    EXPECT(mmay_bytecode_execution_image_init(&image, program, allocator) == MMAY_BYTECODE_IMAGE_OK);
    EXPECT(mmay_bytecode_jit_init(&jit, program, &image, allocator, &fault, false) == MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready && jit.functions[0].code != NULL && jit.functions[1].code != NULL);
    EXPECT(mmay_bytecode_run_frames_reserve(&stack.frames, &stack.capacity, 1, 3, allocator));
    EXPECT(mmay_bytecode_run_frame_init(&stack.frames[0],
        mmay_bytecode_program_function(program, 2), &argument, 1, allocator,
        0, false, false, (MarimayBytecodeSpan){0}) == MMAY_BYTECODE_RUN_OK);
    EXPECT(mmay_bytecode_run_frame_roots_init(&stack.frames[0], &execution, allocator) == MMAY_BYTECODE_RUN_OK);
    stack.count = 1;
    MarimayBytecodeValue result = {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=1234};
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.max_steps = SIZE_MAX; options.max_call_depth = 3; options.execution = &execution;
    mmay_bytecode_jit_start(&jit, program, &image, &stack, allocator, &options, &result);
    MarimayBytecodeRunStatus status;
    bool finished;
    EXPECT(mmay_bytecode_jit_run_stack(&jit, &status, &finished));
    EXPECT(status == MMAY_BYTECODE_RUN_OK && !finished && stack.count == 3);
    MarimayBytecodeRunFrame *leaf = &stack.frames[2];
    EXPECT(leaf->function == mmay_bytecode_program_function(program, 0));
    EXPECT(leaf->block == 0 && leaf->instruction == 4 && result.as.i64 == 1234);
    EXPECT(mmay_managed_heap_collect(&execution.managed_heap) == MMAY_MANAGED_OK);
    written_once(&execution, argument);
    const MarimayBytecodeBlock *block = mmay_vector_at_const(&leaf->function->body.blocks, 0);
    const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(&block->instructions, 4);
    int64_t bounds_index = 0;
    size_t bounds_length = 0;
    EXPECT(mmay_bytecode_run_value_instruction(instruction, leaf->registers, &execution,
        &leaf->function->body, allocator, NULL, NULL, leaf->function->symbol,
        &bounds_index, &bounds_length) == MMAY_BYTECODE_RUN_OK);
    leaf->instruction += 1;
    EXPECT(mmay_managed_heap_collect(&execution.managed_heap) == MMAY_MANAGED_OK);
    EXPECT(mmay_bytecode_jit_run_stack(&jit, &status, &finished));
    EXPECT(status == MMAY_BYTECODE_RUN_OK && finished && result.as.i64 == 0 && stack.count == 1);
    EXPECT(stack.frames[2].registers[2].as.f64_bits == UINT64_C(0x4045000000000000));
    written_once(&execution, argument);
    for (size_t i = stack.count; i != 0; --i)
        mmay_bytecode_run_frame_release(&stack.frames[i - 1], &execution);
    mmay_bytecode_run_frames_dispose(stack.frames, stack.capacity, &allocator, &execution);
    EXPECT(execution.managed_heap.roots == NULL);
    mmay_bytecode_jit_dispose(&jit);
    mmay_bytecode_execution_image_dispose(&image);
    mmay_bytecode_execution_dispose(&execution);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    EXPECT(fault.live_mappings == 0 && fault.live_bytes == 0);
}

static size_t full_run(const MarimayBytecodeProgram *program, size_t failure,
    bool meter, bool bounds) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&failing);
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeValue argument = {0};
    execution_init(&execution, allocator, &argument, bounds ? 0 : 1);
    MarimayBytecodePanic panic = {0};
    EXPECT(mmay_bytecode_panic_init(&panic, allocator));
    failing.attempts = 0; failing.failure_index = failure;
    MarimayBytecodeValue result = {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=1234};
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.execution = &execution; options.max_steps = meter ? 20 : SIZE_MAX;
    options.max_call_depth = 3;
    options.panic = &panic;
    MarimayBytecodeRunStatus status = mmay_bytecode_run_program(program, &argument, 1,
        allocator, options, &result);
    if (failure < failing.attempts) {
        EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED && result.as.i64 == 1234);
    } else if (bounds) {
        EXPECT(status == MMAY_BYTECODE_RUN_PANIC && result.as.i64 == 1234);
        EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
        EXPECT(panic.span.start == 51 && panic.span.end == 52 && panic.frames.length == 3);
        const MarimayBytecodeProgramFunction *leaf = mmay_bytecode_program_function(program, 0);
        EXPECT(memcmp(&panic.symbol, &leaf->symbol, sizeof(panic.symbol)) == 0);
        for (size_t id = 1; id < 3; ++id) {
            const MarimayBytecodePanicFrame *frame = mmay_bytecode_panic_frame(&panic, id);
            const MarimayBytecodeProgramFunction *function = mmay_bytecode_program_function(program, id);
            EXPECT(frame != NULL && memcmp(&frame->symbol, &function->symbol, sizeof(frame->symbol)) == 0);
            EXPECT(frame->call_span.start == (id == 1 ? 40 : 30));
            EXPECT(frame->call_span.end == frame->call_span.start + 1);
        }
    } else {
        EXPECT(status == MMAY_BYTECODE_RUN_OK && result.as.i64 == 0);
        written_once(&execution, argument);
    }
    EXPECT(execution.managed_heap.roots == NULL);
    size_t attempts = failing.attempts;
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    return attempts;
}

int main(void) {
    if (!mmay_code_memory_supported()) return 77;
    MarimayBytecodeProgram program = {0};
    fixture(&program);
    native_exit(&program);
    for (size_t mode = 0; mode < 2; ++mode) {
        for (size_t bounds = 0; bounds < 2; ++bounds) {
            size_t attempts = full_run(&program, SIZE_MAX, mode != 0, bounds != 0);
            for (size_t index = 0; index <= attempts; ++index)
                full_run(&program, index, mode != 0, bounds != 0);
            printf("JIT managed calls (%s, %s): %zu allocation failure indices, zero counters: passed\n",
                mode ? "metered" : "unmetered", bounds ? "panic frames" : "forced GC/write once", attempts);
        }
    }
    mmay_bytecode_program_dispose(&program);
    return 0;
}
