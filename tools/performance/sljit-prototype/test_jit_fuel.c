#include "vm_jit_internal.h"
#include <stdio.h>
#include <stdlib.h>

#define EXPECT(condition) do { if (!(condition)) { fprintf(stderr, "JIT fuel mode failed: %s at %d\n", #condition, __LINE__); exit(1); } } while (0)

/* A terminating scalar loop with a retained backedge, using borrowed metadata
 * to isolate tier admission/accounting from the verifier's separate contract. */
static size_t run(bool meter, size_t maximum, bool has_shared, size_t shared) {
    MarimayBytecodeType types[] = {MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_BOOL};
    MarimayBytecodeProgramFunction function = {0};
    function.body.register_types = (MarimayVector){.data=types, .length=3};
    MarimayBytecodeImageOperation operations[] = {
        {.opcode=MMAY_OPCODE_CONST_I64},
        {.opcode=MMAY_OPCODE_CONST_I64, .first=1, .immediate=1, .source_instruction=1},
        {.opcode=MMAY_OPCODE_CONST_BOOL, .first=2, .source_instruction=2},
        {.opcode=MMAY_OPCODE_JUMP, .first=4, .source_instruction=3},
        {.opcode=MMAY_OPCODE_I64_ADD_CHECKED, .third=1, .source_block=1},
        {.opcode=MMAY_OPCODE_BRANCH_BOOL, .first=2, .second=4, .third=6, .source_block=1, .source_instruction=1},
        {.opcode=MMAY_OPCODE_RETURN, .source_block=2},
    };
    uint32_t starts[] = {0, 4, 6};
    MarimayBytecodeImageFunction image_function = {.operations=operations,
        .block_starts=starts, .operation_count=7, .block_count=3};
    MarimayBytecodeExecutionImage image = {.functions=&image_function, .function_count=1};
    MarimayBytecodeProgram program = {.functions={.data=&function, .length=1}};
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), SIZE_MAX);
    MarimayCodeMemoryFault fault = {SIZE_MAX, 0, 0, 0};
    MarimayBytecodeJit jit = {0};
    EXPECT(mmay_bytecode_jit_init(&jit, &program, &image,
        mmay_fail_allocator(&failing), &fault, meter) == MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready && jit.meters_fuel == meter);
    size_t code_bytes = jit.functions[0].code_bytes;
    MarimayBytecodeValue registers[] = {
        {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=91},
        {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=92},
        {.type=MMAY_BYTECODE_TYPE_BOOL, .as.boolean=true},
    };
    size_t block = 0, instruction = 0, steps = 0, initial_shared = shared;
    MarimayBytecodeRunOptions options = {.max_steps=maximum,
        .remaining_shared_steps=has_shared ? &shared : NULL};
    mmay_bytecode_jit_run(&jit, 0, &image_function, &function.body,
        registers, NULL, &block, &instruction, &steps, options);
    if (!meter && (maximum != SIZE_MAX || has_shared)) {
        EXPECT(registers[0].as.i64 == 91 && registers[1].as.i64 == 92 && registers[2].as.boolean);
        EXPECT(block == 0 && instruction == 0 && steps == 0 && shared == initial_shared);
    } else {
        EXPECT(registers[0].as.i64 == 1 && registers[1].as.i64 == 1 && !registers[2].as.boolean);
        EXPECT(block == 2 && instruction == 0);
        EXPECT(steps == (maximum == SIZE_MAX ? 0 : 6));
        EXPECT(shared == initial_shared - (has_shared ? 6 : 0));
    }
    mmay_bytecode_jit_dispose(&jit);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    EXPECT(fault.live_mappings == 0 && fault.live_bytes == 0);
    return code_bytes;
}

int main(void) {
    if (!mmay_code_memory_supported()) return 77;
    size_t metered = run(true, 7, true, 7);
    run(true, 7, false, 0);
    run(true, SIZE_MAX, true, SIZE_MAX);
    run(true, SIZE_MAX, false, 0);
    run(false, 7, false, 0);
    run(false, SIZE_MAX, true, SIZE_MAX);
    run(false, SIZE_MAX, true, 7);
    run(false, SIZE_MAX, true, 0);
    size_t unmetered = run(false, SIZE_MAX, false, 0);
    printf("JIT fuel mode: exact metered exit, reject every shared/finite counter in unmetered mode, zero counters: passed (%zu / %zu code bytes)\n", metered, unmetered);
    return 0;
}
