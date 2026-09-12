#include "test_bytecode_vm_support.h"
#include "test_callable_engine_support.h"
#include "mmay/runtime/array_view.h"

static bool mmay_hot_program(MarimayBytecodeFunction *function,
    MarimayBytecodeProgram *program) {
    MarimaySymbolId symbol = {{1}};
    uint32_t id = UINT32_MAX;
    return mmay_bytecode_program_init(program, mmay_system_allocator(), 1,
            mmay_bytecode_program_default_options())
        && mmay_bytecode_program_take_function(program, symbol, function, &id)
        && mmay_bytecode_program_set_entry(program, symbol)
        && mmay_bytecode_program_verify(program) == MMAY_BYTECODE_PROGRAM_VERIFY_OK;
}

/* The single-function entry retains ordinary dispatch and is an independent
 * reference for the optimized program entry, including every fuel boundary. */
static void mmay_hot_compare_scalar(const MarimayBytecodeProgram *program,
    const MarimayBytecodeValue *arguments, size_t argument_count,
    size_t frame_limit, size_t shared_limit, bool shared) {
    const MarimayBytecodeFunction *function =
        &mmay_bytecode_program_function(program, 0)->body;
    MarimayBytecodeRunStatus statuses[2];
    MarimayBytecodeTrap traps[2] = {{0}};
    MarimayBytecodeValue results[2];
    size_t remaining[2] = {shared_limit, shared_limit};
    for (unsigned engine = 0; engine < 2; engine += 1) {
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        options.max_steps = frame_limit;
        options.remaining_shared_steps = shared ? &remaining[engine] : NULL;
        options.trap = &traps[engine];
        results[engine] = mmay_test_vm_i64(1234);
        statuses[engine] = engine == 0
            ? mmay_bytecode_run(function, arguments, argument_count,
                mmay_system_allocator(), options, &results[engine])
            : mmay_bytecode_run_program(program, arguments, argument_count,
                mmay_system_allocator(), options, &results[engine]);
    }
    MMAY_EXPECT(statuses[0] == statuses[1]);
    MMAY_EXPECT(results[0].type == results[1].type);
    MMAY_EXPECT(results[0].as.i64 == results[1].as.i64);
    MMAY_EXPECT(remaining[0] == remaining[1]);
    MMAY_EXPECT(traps[0].has_span == traps[1].has_span);
    if (traps[0].has_span) {
        MMAY_EXPECT(traps[0].span.start == traps[1].span.start);
        MMAY_EXPECT(traps[0].span.end == traps[1].span.end);
    }
}

static void mmay_hot_scalar_boundaries(void) {
    MarimayBytecodeFunction function = {0};
    MarimayBytecodeProgram program = {0};
    MarimayBytecodeValue args[3] = {
        mmay_test_vm_bool(true), mmay_test_vm_i64(40), mmay_test_vm_i64(2)
    };
    MMAY_EXPECT(mmay_test_vm_build_all_opcodes(&function, mmay_system_allocator()));
    MMAY_EXPECT(mmay_hot_program(&function, &program));
    for (unsigned branch = 0; branch < 2; branch += 1) {
        args[0].as.boolean = branch != 0;
        for (size_t fuel = 1; fuel <= 25; fuel += 1) {
            mmay_hot_compare_scalar(&program, args, 3, fuel, 0, false);
            mmay_hot_compare_scalar(&program, args, 3, SIZE_MAX, fuel, true);
            mmay_hot_compare_scalar(&program, args, 3, fuel, fuel + 1, true);
            mmay_hot_compare_scalar(&program, args, 3, fuel + 1, fuel, true);
        }
    }
    mmay_bytecode_program_dispose(&program);
    static const MarimayOpcode operations[] = {
        MMAY_OPCODE_I64_ADD_CHECKED, MMAY_OPCODE_I64_SUBTRACT_CHECKED,
        MMAY_OPCODE_I64_MULTIPLY_CHECKED, MMAY_OPCODE_I64_DIVIDE_CHECKED,
        MMAY_OPCODE_I64_REMAINDER_CHECKED
    };
    static const int64_t values[] = {INT64_MIN, INT64_MAX, -1, 0, 1, 37};
    for (size_t op = 0; op < sizeof(operations) / sizeof(operations[0]); op += 1) {
        MMAY_EXPECT(mmay_test_vm_build_binary(&function,
            mmay_system_allocator(), operations[op]));
        MMAY_EXPECT(mmay_hot_program(&function, &program));
        for (size_t a = 0; a < sizeof(values) / sizeof(values[0]); a += 1) {
            for (size_t b = 0; b < sizeof(values) / sizeof(values[0]); b += 1) {
                MarimayBytecodeValue pair[] = {
                    mmay_test_vm_i64(values[a]), mmay_test_vm_i64(values[b])
                };
                mmay_hot_compare_scalar(&program, pair, 2, SIZE_MAX, 0, false);
                mmay_hot_compare_scalar(&program, pair, 2, SIZE_MAX, 1, true);
            }
        }
        mmay_bytecode_program_dispose(&program);
    }
    mmay_bytecode_function_dispose(&function);
}

/* A backward edge repeatedly changes both cursor fields; the divide instruction
 * can also fail after earlier loop iterations have successfully committed. */
static void mmay_hot_loop_boundaries(void) {
    MarimayBytecodeFunction function = {0};
    MarimayBytecodeProgram program = {0};
    const MarimayBytecodeType parameters[] = {MMAY_BYTECODE_TYPE_I64};
    const MarimayBytecodeType locals[] = {
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_BOOL
    };
    uint32_t block = UINT32_MAX;
    uint64_t span = 1000;
    MMAY_EXPECT(mmay_bytecode_function_init(&function, mmay_system_allocator(),
        MMAY_BYTECODE_TYPE_I64, 1, parameters, 1,
        mmay_bytecode_function_default_options()));
    MMAY_EXPECT(mmay_test_vm_add_registers(&function, locals, 4));
    for (size_t index = 0; index < 4; index += 1) {
        MMAY_EXPECT(mmay_bytecode_add_block(&function, &block));
    }
    MMAY_EXPECT(mmay_test_vm_append(&function, 0, MMAY_OPCODE_CONST_I64, 1, 4, 0, &span));
    MMAY_EXPECT(mmay_test_vm_append(&function, 0, MMAY_OPCODE_CONST_I64, 2, 1, 0, &span));
    MMAY_EXPECT(mmay_test_vm_append(&function, 0, MMAY_OPCODE_JUMP, 1, 0, 0, &span));
    MMAY_EXPECT(mmay_test_vm_append(&function, 1, MMAY_OPCODE_I64_LESS, 4, 0, 1, &span));
    MMAY_EXPECT(mmay_test_vm_append(&function, 1, MMAY_OPCODE_BRANCH_BOOL, 4, 2, 3, &span));
    MMAY_EXPECT(mmay_test_vm_append(&function, 2, MMAY_OPCODE_I64_DIVIDE_CHECKED, 3, 0, 1, &span));
    MMAY_EXPECT(mmay_test_vm_append(&function, 2, MMAY_OPCODE_I64_SUBTRACT_CHECKED, 1, 1, 2, &span));
    MMAY_EXPECT(mmay_test_vm_append(&function, 2, MMAY_OPCODE_JUMP, 1, 0, 0, &span));
    MMAY_EXPECT(mmay_test_vm_append(&function, 3, MMAY_OPCODE_RETURN, 1, 0, 0, &span));
    MMAY_EXPECT(mmay_hot_program(&function, &program));
    for (int64_t input = -1; input <= 4; input += 1) {
        MarimayBytecodeValue argument = mmay_test_vm_i64(input);
        for (size_t fuel = 0; fuel <= 26; fuel += 1) {
            mmay_hot_compare_scalar(&program, &argument, 1, fuel, 0, false);
            mmay_hot_compare_scalar(&program, &argument, 1, SIZE_MAX, fuel, true);
            mmay_hot_compare_scalar(&program, &argument, 1, fuel, fuel + 1, true);
            mmay_hot_compare_scalar(&program, &argument, 1, fuel + 1, fuel, true);
        }
        mmay_hot_compare_scalar(&program, &argument, 1, SIZE_MAX, 0, false);
        mmay_hot_compare_scalar(&program, &argument, 1, SIZE_MAX, SIZE_MAX, true);
    }
    mmay_bytecode_program_dispose(&program);
    mmay_bytecode_function_dispose(&function);
}

static bool mmay_hot_array_function(MarimayBytecodeFunction *function) {
    const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_ARRAY_F64, MMAY_BYTECODE_TYPE_I64
    };
    const MarimayBytecodeType locals[] = {
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_F64, MMAY_BYTECODE_TYPE_ARRAY_F64
    };
    uint32_t block = UINT32_MAX;
    uint64_t span = 100;
    return mmay_bytecode_function_init(function, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_F64, 1, parameters, 2,
            mmay_bytecode_function_default_options())
        && mmay_test_vm_add_registers(function, locals, 4)
        && mmay_bytecode_add_block(function, &block)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_CONST_F64,
            3, UINT64_C(0x4022000000000000), 0, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_ARRAY_LENGTH, 2, 0, 0, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 5, 0, 1, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, 4, 5, 1, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_ARRAY_SET_IN_BOUNDS, 5, 1, 3, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_RETURN, 4, 0, 0, &span);
}

typedef struct MarimayHotArrayObservation {
    MarimayBytecodeRunStatus status;
    MarimayBytecodeValue result;
    MarimayBytecodeTrap trap;
    MarimayBytecodeSpan panic_span;
    const char *panic_code;
    uint64_t bits;
    uint64_t generation;
    size_t remaining;
} MarimayHotArrayObservation;

static MarimayHotArrayObservation mmay_hot_array_observe(
    const MarimayBytecodeProgram *program, bool hot, int64_t index,
    size_t fuel, bool shared, bool saturated) {
    MarimayHotArrayObservation observed = {0};
    MarimayFailAllocator state;
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodePanic panic = {0};
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue arguments[] = {{0}, mmay_test_vm_i64(index)};
    MarimayManagedCell cell = {0};
    MarimayManagedArrayView view;
    size_t length = 0;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, allocator,
        mmay_bytecode_execution_default_options()));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, allocator));
    arguments[0].type = MMAY_BYTECODE_TYPE_ARRAY_F64;
    cell.f64_bits = UINT64_C(0x3ff0000000000000);
    MMAY_EXPECT(mmay_managed_array_new(&execution.managed_heap,
        &mmay_managed_array_f64_descriptor, &cell, 1,
        &arguments[0].as.managed_reference) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_view_bind(&execution.managed_heap,
        arguments[0].as.managed_reference, &mmay_managed_array_f64_descriptor,
        &view) == MMAY_MANAGED_OK);
    if (saturated) *view.mutation_generation = UINT64_MAX;
    options.execution = &execution;
    options.panic = &panic;
    options.trap = &observed.trap;
    options.max_steps = shared ? SIZE_MAX : fuel;
    observed.remaining = fuel;
    options.remaining_shared_steps = shared ? &observed.remaining : NULL;
    observed.result = mmay_test_vm_i64(1234);
    observed.status = hot
        ? mmay_bytecode_run_program(program, arguments, 2, allocator, options, &observed.result)
        : mmay_bytecode_run(&mmay_bytecode_program_function(program, 0)->body,
            arguments, 2, allocator, options, &observed.result);
    observed.panic_code = panic.code;
    observed.panic_span = panic.span;
    MMAY_EXPECT(mmay_managed_array_get(&execution.managed_heap,
        arguments[0].as.managed_reference, 0, &cell, &length) == MMAY_MANAGED_OK);
    observed.bits = cell.f64_bits;
    MMAY_EXPECT(mmay_managed_array_mutation_generation(&execution.managed_heap,
        arguments[0].as.managed_reference, &observed.generation) == MMAY_MANAGED_OK);
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    return observed;
}

static void mmay_hot_array_boundaries(void) {
    MarimayBytecodeFunction function = {0};
    MarimayBytecodeProgram program = {0};
    MMAY_EXPECT(mmay_hot_array_function(&function));
    MMAY_EXPECT(mmay_hot_program(&function, &program));
    for (int64_t index = -1; index <= 1; index += 1) {
        for (size_t fuel = 1; fuel <= 7; fuel += 1) {
            for (unsigned flags = 0; flags < 4; flags += 1) {
                MarimayHotArrayObservation cold = mmay_hot_array_observe(
                    &program, false, index, fuel, (flags & 1) != 0, (flags & 2) != 0);
                MarimayHotArrayObservation hot = mmay_hot_array_observe(
                    &program, true, index, fuel, (flags & 1) != 0, (flags & 2) != 0);
                MMAY_EXPECT(cold.status == hot.status);
                MMAY_EXPECT(cold.result.type == hot.result.type);
                MMAY_EXPECT(cold.result.as.integer_bits == hot.result.as.integer_bits);
                MMAY_EXPECT(cold.bits == hot.bits && cold.generation == hot.generation);
                MMAY_EXPECT(cold.remaining == hot.remaining);
                MMAY_EXPECT(cold.trap.has_span == hot.trap.has_span);
                if (cold.trap.has_span) {
                    MMAY_EXPECT(cold.trap.span.start == hot.trap.span.start);
                    MMAY_EXPECT(cold.trap.span.end == hot.trap.span.end);
                }
                MMAY_EXPECT((cold.panic_code == NULL) == (hot.panic_code == NULL));
                if (cold.panic_code != NULL && hot.panic_code != NULL) {
                    MMAY_EXPECT(strcmp(cold.panic_code, hot.panic_code) == 0);
                    MMAY_EXPECT(cold.panic_span.start == hot.panic_span.start);
                    MMAY_EXPECT(cold.panic_span.end == hot.panic_span.end);
                }
            }
        }
    }
    mmay_bytecode_program_dispose(&program);
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_vm_hot_regions(void) {
    mmay_hot_scalar_boundaries();
    mmay_hot_loop_boundaries();
    mmay_hot_array_boundaries();
    static const char source[] =
        "fn pressure():void{const make=():[]int=>[99];const _=make();}"
        "fn mutate(values:[]int):void{const _=push(values,7);values[0]=12;}"
        "fn main():int{let values=[1,2];const alias=values;let sum=0;"
        "for i in 0..8{sum+=values[0];values[0]+=i;"
        "if i%2==0{mutate(alias);}pressure();}"
        "const saved=values;values=[20];pressure();"
        "const tiny:[]i8=[3,4];tiny[0]=5;"
        "if tiny[0]!=5{panic(\"narrow fallback\");}"
        "const nested=[[2]];if nested[0][0]!=2{panic(\"reference fallback\");}"
        "return sum+saved[0]+values[0]+saved.length();}";
    mmay_test_callable_engines(source, 139, 1, 1);
    static const char cache_source[] =
        "fn main():int{const a:[]u32=[1];const b:[]u32=[2];"
        "const c:[]u32=[4];const d:[]u32=[8];const e:[]u32=[16];"
        "const f:[]i32=[-32];const g=[64];const alias=a;let sum:u32=0;"
        "for i in 0..8{sum+=a[0]+b[0]+c[0]+d[0]+e[0];"
        "if f[0]!=-32||g[0]!=64{panic(\"cache layout\");}alias[0]+=1;}"
        "if sum+a[0]!=285{panic(\"cache identity\");}return 1053;}";
    /* Distinct references with equal metadata, different scalar layouts, an
     * alias write and more live arrays than the four-entry region cache. */
    mmay_test_callable_engines(cache_source, 1053, 0, 0);
    static const char rebind_source[] =
        "fn main():int{const a=[1];const b=[2];const c=[4];const d=[8];"
        "let selected=c;let sum=0;for i in 0..8{sum+=a[0]+b[0]+c[0];"
        "sum+=selected[0];if i%2==0{selected=a;}else{selected=c;}"
        "selected[0]+=1;sum+=selected[0];if i==3{selected=d;}}"
        "return sum+a[0]+c[0]+d[0];}";
    /* Revisit a recent entry, then rebind the same typed register and mutate
     * through it. Complete identity must win over either access history. */
    mmay_test_callable_engines(rebind_source, 183, 0, 0);
}
