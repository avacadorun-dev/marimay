#include "vm_jit_internal.h"
#include <stdio.h>
#include <stdlib.h>

#define EXPECT(condition) do { if (!(condition)) { fprintf(stderr, "JIT calls failed: %s at %d\n", #condition, __LINE__); exit(1); } } while (0)

static void append(MarimayBytecodeFunction *function, uint32_t block,
    MarimayOpcode opcode, uint64_t first, uint64_t second, uint64_t third,
    uint64_t span, int64_t immediate) {
    MarimayBytecodeInstruction instruction = {.opcode=opcode,
        .span={.start=span, .end=span + 1}};
    instruction.operands[0].unsigned_value = first;
    instruction.operands[1].unsigned_value = second;
    instruction.operands[2].unsigned_value = third;
    if (opcode == MMAY_OPCODE_CONST_I64) instruction.operands[1].signed_value = immediate;
    EXPECT(mmay_bytecode_append_instruction(function, block, &instruction));
}

/* Only entry has a backedge. The two callees require transitive admission. */
static void fixture(MarimayBytecodeProgram *program, bool overflow) {
    MarimayAllocator allocator = mmay_system_allocator();
    EXPECT(mmay_bytecode_program_init(program, allocator, 1,
        mmay_bytecode_program_default_options()));
    for (uint32_t id = 0; id < 3; ++id) {
        MarimayBytecodeFunction function = {0};
        uint32_t reg = 0, block = 0, added = 0;
        EXPECT(mmay_bytecode_function_init(&function, allocator, MMAY_BYTECODE_TYPE_I64,
            1, NULL, 0, mmay_bytecode_function_default_options()));
        EXPECT(mmay_bytecode_add_register(&function, MMAY_BYTECODE_TYPE_I64, &reg));
        EXPECT(mmay_bytecode_add_block(&function, &block));
        if (id == 2) {
            EXPECT(mmay_bytecode_add_register(&function, MMAY_BYTECODE_TYPE_BOOL, &reg));
            EXPECT(mmay_bytecode_add_block(&function, &block));
            append(&function, 0, MMAY_OPCODE_CONST_BOOL, reg, 0, 0, 28, 0);
            append(&function, 0, MMAY_OPCODE_BRANCH_BOOL, reg, 0, block, 29, 0);
        }
        if (id == 0) {
            append(&function, block, MMAY_OPCODE_CONST_I64, 0, 0, 0, 50,
                overflow ? INT64_MIN : 42);
            if (overflow) append(&function, block, MMAY_OPCODE_I64_NEGATE_CHECKED,
                0, 0, 0, 51, 0);
        } else {
            MarimayBytecodeCallId call;
            MarimaySymbolId callee = {{(unsigned char)(id * 0x10)}};
            EXPECT(mmay_bytecode_add_call(&function, callee, MMAY_BYTECODE_TYPE_I64,
                NULL, 0, &call));
            append(&function, block, MMAY_OPCODE_CALL_DIRECT, 0, call, 0,
                id == 1 ? 40 : 30, 0);
        }
        append(&function, block, MMAY_OPCODE_RETURN, 0, 0, 0,
            id == 0 ? (overflow ? 52 : 51) : id == 1 ? 41 : 31, 0);
        MarimaySymbolId symbol = {{(unsigned char)((id + 1) * 0x10)}};
        EXPECT(mmay_bytecode_program_take_function(program, symbol, &function, &added));
        EXPECT(added == id);
        if (id == 2) EXPECT(mmay_bytecode_program_set_entry(program, symbol));
    }
    EXPECT(mmay_bytecode_program_verify(program) == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static size_t native_entry(const MarimayBytecodeProgram *program, bool overflow,
    size_t failure, size_t maximum_depth) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), failure);
    MarimayAllocator allocator = mmay_fail_allocator(&failing);
    MarimayCodeMemoryFault fault = {SIZE_MAX, 0, 0, 0};
    MarimayBytecodeExecutionImage image = {0};
    MarimayBytecodeJit jit = {0};
    MarimayBytecodeRunStack stack = {0};
    MarimayBytecodeValue result = {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=1234};
    MarimayBytecodeTrap trap = {0};
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.max_steps = SIZE_MAX;
    options.max_call_depth = maximum_depth;
    options.trap = &trap;
    MarimayBytecodeRunStatus status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    MarimayBytecodeImageStatus image_status = mmay_bytecode_execution_image_init(
        &image, program, allocator);
    EXPECT(image_status == MMAY_BYTECODE_IMAGE_OK
        || image_status == MMAY_BYTECODE_IMAGE_ALLOCATION_FAILED);
    if (image_status != MMAY_BYTECODE_IMAGE_OK) goto cleanup;
    status = mmay_bytecode_jit_init(&jit, program, &image, allocator, &fault, false);
    if (status != MMAY_BYTECODE_RUN_OK) goto cleanup;
    EXPECT(jit.ready && jit.function_count == 3);
    EXPECT(jit.state->activation_metadata != NULL && jit.state->activation_metadata_bytes != 0);
    for (size_t id = 0; id < 3; ++id) {
        EXPECT(jit.functions[id].code != NULL);
        EXPECT(jit.functions[id].frame_layout.function == mmay_bytecode_program_function(program, id));
        EXPECT(jit.functions[id].frame_layout.initial_registers != NULL);
        if (id != 0) {
            EXPECT(jit.functions[id].calls[0].caller == mmay_bytecode_program_function(program, id));
            EXPECT(jit.functions[id].calls[0].callee == mmay_bytecode_program_function(program, id - 1));
            EXPECT(jit.functions[id].calls[0].layout == &jit.functions[id - 1].frame_layout);
        }
    }
    status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    if (!mmay_bytecode_run_frames_reserve(&stack.frames, &stack.capacity, 1,
        options.max_call_depth, allocator)) goto cleanup;
    status = mmay_bytecode_run_frame_init(&stack.frames[0],
        mmay_bytecode_program_function(program, 2), NULL, 0, allocator,
        0, false, false, (MarimayBytecodeSpan){0});
    if (status != MMAY_BYTECODE_RUN_OK) goto cleanup;
    status = mmay_bytecode_run_frame_roots_init(&stack.frames[0], NULL, allocator);
    if (status != MMAY_BYTECODE_RUN_OK) goto cleanup;
    stack.count = 1;
    bool finished = false;
    mmay_bytecode_jit_start(&jit, program, &image, &stack, allocator, &options, &result);
    EXPECT(mmay_bytecode_jit_run_stack(&jit, &status, &finished));
    if (status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED) goto cleanup;
    if (maximum_depth == 2) {
        EXPECT(status == MMAY_BYTECODE_RUN_STEP_LIMIT && !finished);
        EXPECT(trap.has_span && trap.span.start == 40 && stack.count == 2);
        EXPECT(result.as.i64 == 1234);
    } else if (overflow) {
        EXPECT(status == MMAY_BYTECODE_RUN_OK && !finished && stack.count == 3);
        EXPECT(stack.frames[2].function == mmay_bytecode_program_function(program, 0));
        EXPECT(stack.frames[2].block == 0 && stack.frames[2].instruction == 1);
        EXPECT(stack.frames[2].registers[0].as.i64 == INT64_MIN);
        EXPECT(stack.frames[0].block == 1 && stack.frames[0].instruction == 1);
        EXPECT(stack.frames[1].block == 0 && stack.frames[1].instruction == 1);
        EXPECT(result.as.i64 == 1234 && !trap.has_span);
    } else {
        EXPECT(status == MMAY_BYTECODE_RUN_OK && finished && stack.count == 1);
        EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    }
cleanup:
    if (failure < failing.attempts) {
        EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED && result.as.i64 == 1234);
    } else EXPECT(status == MMAY_BYTECODE_RUN_OK || status == MMAY_BYTECODE_RUN_STEP_LIMIT);
    mmay_bytecode_run_frames_dispose(stack.frames, stack.capacity, &allocator, NULL);
    mmay_bytecode_jit_dispose(&jit);
    mmay_bytecode_execution_image_dispose(&image);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    EXPECT(fault.live_mappings == 0 && fault.live_bytes == 0);
    return failing.attempts;
}

static void observable(const MarimayBytecodeProgram *program, bool overflow) {
    for (size_t mode = 0; mode < 3; ++mode) {
        MarimayBytecodeTrap trap = {0};
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        size_t shared = 8;
        options.max_steps = mode == 1 ? 4 : SIZE_MAX;
        options.remaining_shared_steps = mode == 2 ? &shared : NULL;
        options.max_call_depth = 3;
        options.trap = &trap;
        MarimayBytecodeValue result = {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=1234};
        EXPECT(mmay_bytecode_run_program(program, NULL, 0, mmay_system_allocator(),
            options, &result) == (overflow ? MMAY_BYTECODE_RUN_OVERFLOW : MMAY_BYTECODE_RUN_OK));
        EXPECT(result.as.i64 == (overflow ? 1234 : 42));
        EXPECT(trap.has_span == overflow);
        if (overflow) EXPECT(trap.span.start == 51 && trap.span.end == 52);
        if (mode == 2) EXPECT(shared == (overflow ? 2 : 0));
    }
}

static void recursion(size_t maximum_depth) {
    MarimayAllocator system = mmay_system_allocator();
    MarimayBytecodeProgram program = {0};
    MarimayBytecodeFunction function = {0};
    MarimaySymbolId symbol = {{0x10}};
    MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_I64;
    EXPECT(mmay_bytecode_program_init(&program, system, 1,
        mmay_bytecode_program_default_options()));
    EXPECT(mmay_bytecode_function_init(&function, system, MMAY_BYTECODE_TYPE_I64,
        1, &parameter, 1, mmay_bytecode_function_default_options()));
    uint32_t added = 0;
    for (size_t i = 1; i < 6; ++i)
        EXPECT(mmay_bytecode_add_register(&function,
            i < 4 ? MMAY_BYTECODE_TYPE_I64 : MMAY_BYTECODE_TYPE_BOOL, &added));
    for (size_t i = 0; i < 4; ++i) EXPECT(mmay_bytecode_add_block(&function, &added));
    MarimayBytecodeCallArgument argument = {.register_id=2, .type=MMAY_BYTECODE_TYPE_I64};
    MarimayBytecodeCallId call;
    EXPECT(mmay_bytecode_add_call(&function, symbol, MMAY_BYTECODE_TYPE_I64,
        &argument, 1, &call));
    append(&function, 0, MMAY_OPCODE_CONST_BOOL, 5, 0, 0, 100, 0);
    append(&function, 0, MMAY_OPCODE_BRANCH_BOOL, 5, 0, 1, 101, 0);
    append(&function, 1, MMAY_OPCODE_CONST_I64, 1, 0, 0, 102, 1);
    append(&function, 1, MMAY_OPCODE_I64_LESS_EQUAL, 4, 0, 1, 103, 0);
    append(&function, 1, MMAY_OPCODE_BRANCH_BOOL, 4, 2, 3, 104, 0);
    append(&function, 2, MMAY_OPCODE_RETURN, 1, 0, 0, 105, 0);
    append(&function, 3, MMAY_OPCODE_I64_SUBTRACT_CHECKED, 2, 0, 1, 106, 0);
    append(&function, 3, MMAY_OPCODE_CALL_DIRECT, 3, call, 0, 107, 0);
    append(&function, 3, MMAY_OPCODE_RETURN, 3, 0, 0, 108, 0);
    EXPECT(mmay_bytecode_program_take_function(&program, symbol, &function, &added));
    EXPECT(mmay_bytecode_program_set_entry(&program, symbol));
    EXPECT(mmay_bytecode_program_verify(&program) == MMAY_BYTECODE_PROGRAM_VERIFY_OK);

    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, system, SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&failing);
    MarimayCodeMemoryFault fault = {SIZE_MAX, 0, 0, 0};
    MarimayBytecodeExecutionImage image = {0};
    MarimayBytecodeJit jit = {0};
    EXPECT(mmay_bytecode_execution_image_init(&image, &program, allocator) == MMAY_BYTECODE_IMAGE_OK);
    EXPECT(mmay_bytecode_jit_init(&jit, &program, &image, allocator, &fault, false) == MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready);
    MarimayBytecodeRunStack stack = {0};
    EXPECT(mmay_bytecode_run_frames_reserve(&stack.frames, &stack.capacity, 1,
        maximum_depth, allocator));
    MarimayBytecodeValue input = {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=10000};
    EXPECT(mmay_bytecode_run_frame_init(&stack.frames[0],
        mmay_bytecode_program_function(&program, 0), &input, 1, allocator,
        0, false, false, (MarimayBytecodeSpan){0}) == MMAY_BYTECODE_RUN_OK);
    EXPECT(mmay_bytecode_run_frame_roots_init(&stack.frames[0], NULL, allocator) == MMAY_BYTECODE_RUN_OK);
    stack.count = 1;
    MarimayBytecodeValue result = {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=1234};
    MarimayBytecodeTrap trap = {0};
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.max_steps = SIZE_MAX;
    options.max_call_depth = maximum_depth;
    options.trap = &trap;
    mmay_bytecode_jit_start(&jit, &program, &image, &stack, allocator, &options, &result);
    MarimayBytecodeRunStatus status;
    bool finished;
    EXPECT(mmay_bytecode_jit_run_stack(&jit, &status, &finished));
    if (maximum_depth >= 10000) {
        EXPECT(status == MMAY_BYTECODE_RUN_OK && finished && stack.count == 1);
        EXPECT(result.as.i64 == 1 && !trap.has_span);
    } else {
        EXPECT(status == MMAY_BYTECODE_RUN_STEP_LIMIT && !finished && stack.count == maximum_depth);
        EXPECT(result.as.i64 == 1234 && trap.has_span && trap.span.start == 107);
    }
    mmay_bytecode_run_frames_dispose(stack.frames, stack.capacity, &allocator, NULL);
    mmay_bytecode_jit_dispose(&jit);
    mmay_bytecode_execution_image_dispose(&image);
    mmay_bytecode_program_dispose(&program);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    EXPECT(fault.live_mappings == 0 && fault.live_bytes == 0);
}

int main(void) {
    if (!mmay_code_memory_supported()) return 77;
    for (size_t kind = 0; kind < 2; ++kind) {
        MarimayBytecodeProgram program = {0};
        fixture(&program, kind != 0);
        size_t allocations = native_entry(&program, kind != 0, SIZE_MAX, 3);
        for (size_t index = 0; index <= allocations; ++index)
            native_entry(&program, kind != 0, index, 3);
        native_entry(&program, kind != 0, SIZE_MAX, 2);
        observable(&program, kind != 0);
        mmay_bytecode_program_dispose(&program);
        printf("JIT nested calls (%s): one native entry, logical frames/exit PC, %zu allocation failure indices, zero counters: passed\n",
            kind ? "overflow" : "return", allocations);
    }
    recursion(10000);
    recursion(100);
    puts("JIT recursion: 10000 logical frames in one native entry, argument transport, bounded depth and zero counters: passed");
    return 0;
}
