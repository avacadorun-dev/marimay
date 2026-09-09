#include "test_bytecode_vm_support.h"
#include "test_callable_engine_support.h"
#include "mmay/runtime/array_scalar_view.h"
#include "../../bootstrap/src/bytecode/vm_hot.h"

typedef struct MarimayScalarArrayCase {
    MarimayBytecodeType type;
    uint64_t before;
    uint64_t after;
} MarimayScalarArrayCase;

/* Prepare an array through the ordinary public VM entry, so descriptor
 * interning has exactly the same owner in both executions under comparison. */
static bool mmay_scalar_array_new(MarimayBytecodeExecution *execution,
    const MarimayScalarArrayCase *test, MarimayBytecodeValue *array,
    MarimayBytecodeFunction *builder) {
    uint32_t layout = UINT32_MAX, reg = UINT32_MAX, block = UINT32_MAX;
    MarimayBytecodeArrayConstructionId construction = MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
    uint64_t span = 800;
    bool built = mmay_bytecode_function_init(builder, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_ARRAY, 1, NULL, 0, mmay_bytecode_function_default_options())
        && mmay_bytecode_add_array_type(builder, test->type, MMAY_BYTECODE_NO_LAYOUT, &layout)
        && mmay_bytecode_add_register(builder, MMAY_BYTECODE_TYPE_ARRAY, &reg)
        && mmay_bytecode_set_register_layout(builder, reg, layout)
        && mmay_bytecode_add_array_construction_with_layout(builder,
            MMAY_BYTECODE_TYPE_ARRAY, layout, NULL, 0, &construction)
        && mmay_bytecode_add_block(builder, &block)
        && mmay_test_vm_append(builder, block, MMAY_OPCODE_ARRAY_NEW, reg, construction, 0, &span)
        && mmay_test_vm_append(builder, block, MMAY_OPCODE_RETURN, reg, 0, 0, &span);
    builder->result_layout = layout;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.execution = execution;
    bool ok = built && mmay_bytecode_run(builder, NULL, 0,
        execution->allocator, options, array) == MMAY_BYTECODE_RUN_OK;
    /* Descriptor entries borrow their function's layout metadata until teardown. */
    if (ok) {
        MarimayManagedCell cell = {0};
        cell.f64_bits = test->before;
        ok = mmay_managed_array_push(&execution->managed_heap,
            array->as.managed_reference, cell) == MMAY_MANAGED_OK;
    }
    return ok;
}

static bool mmay_scalar_array_program(MarimayBytecodeProgram *program,
    const MarimayScalarArrayCase *test, bool proven) {
    MarimayBytecodeFunction function = {0};
    const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_ARRAY, MMAY_BYTECODE_TYPE_I64, test->type
    };
    const MarimayBytecodeType locals[] = {
        MMAY_BYTECODE_TYPE_I64, test->type, MMAY_BYTECODE_TYPE_ARRAY
    };
    uint32_t layout = UINT32_MAX, block = UINT32_MAX, id = UINT32_MAX;
    MarimaySymbolId symbol = {{1}};
    uint64_t span = 900;
    bool ok = mmay_bytecode_function_init(&function, mmay_system_allocator(),
            test->type, 1, parameters, 3, mmay_bytecode_function_default_options())
        && mmay_test_vm_add_registers(&function, locals, 3)
        && mmay_bytecode_add_array_type(&function, test->type, MMAY_BYTECODE_NO_LAYOUT, &layout)
        && mmay_bytecode_set_register_layout(&function, 0, layout)
        && mmay_bytecode_set_register_layout(&function, 5, layout)
        && mmay_bytecode_add_block(&function, &block)
        && mmay_test_vm_append(&function, block, MMAY_OPCODE_ARRAY_LENGTH, 3, 0, 0, &span);
    if (ok && proven) ok = mmay_test_vm_append(&function, block,
        MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 5, 0, 1, &span);
    ok = ok && mmay_test_vm_append(&function, block,
            proven ? MMAY_OPCODE_ARRAY_GET_IN_BOUNDS : MMAY_OPCODE_ARRAY_GET,
            4, proven ? 5 : 0, 1, &span);
    if (ok && !proven) ok = mmay_test_vm_append(&function, block,
        MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 5, 0, 1, &span);
    ok = ok && mmay_test_vm_append(&function, block,
            proven ? MMAY_OPCODE_ARRAY_SET_IN_BOUNDS : MMAY_OPCODE_ARRAY_SET,
            5, 1, 2, &span)
        && mmay_test_vm_append(&function, block, MMAY_OPCODE_RETURN, 4, 0, 0, &span)
        && mmay_bytecode_program_init(program, mmay_system_allocator(), 1,
            mmay_bytecode_program_default_options())
        && mmay_bytecode_program_take_function(program, symbol, &function, &id)
        && mmay_bytecode_program_set_entry(program, symbol);
    if (ok) {
        MarimayBytecodeProgramVerifyStatus status = mmay_bytecode_program_verify(program);
        if (status != MMAY_BYTECODE_PROGRAM_VERIFY_OK) fprintf(stderr, "array fixture type=%u proven=%u verify=%u\n", (unsigned)test->type, (unsigned)proven, (unsigned)status);
        ok = status == MMAY_BYTECODE_PROGRAM_VERIFY_OK;
    }
    mmay_bytecode_function_dispose(&function);
    return ok;
}

typedef struct MarimayScalarArrayObservation {
    MarimayBytecodeRunStatus status;
    MarimayBytecodeValue result;
    MarimayBytecodeTrap trap;
    MarimayBytecodeSpan panic_span;
    const char *panic_code;
    uint64_t bits;
    uint64_t generation;
    size_t remaining;
} MarimayScalarArrayObservation;

static MarimayScalarArrayObservation mmay_scalar_array_observe(
    const MarimayBytecodeProgram *program, const MarimayScalarArrayCase *test,
    bool hot, int64_t index, size_t fuel, bool shared, bool saturated) {
    MarimayScalarArrayObservation observed = {0};
    MarimayFailAllocator state;
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeFunction builder = {0};
    MarimayBytecodePanic panic = {0};
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue arguments[] = {{0}, mmay_test_vm_i64(index), {0}};
    MarimayManagedCell cell = {0};
    const MarimayManagedArrayDescriptor *descriptor = NULL;
    MarimayManagedArrayView view;
    size_t length = 0;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, allocator,
        mmay_bytecode_execution_default_options()));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, allocator));
    MMAY_EXPECT(mmay_scalar_array_new(&execution, test, &arguments[0], &builder));
    arguments[2].type = test->type;
    if (test->type == MMAY_BYTECODE_TYPE_BOOL) arguments[2].as.boolean = test->after != 0;
    else arguments[2].as.integer_bits = test->after;
    MMAY_EXPECT(mmay_managed_array_describe(&execution.managed_heap,
        arguments[0].as.managed_reference, &descriptor) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_view_bind(&execution.managed_heap,
        arguments[0].as.managed_reference, descriptor, &view) == MMAY_MANAGED_OK);
    if (saturated) *view.mutation_generation = UINT64_MAX;
    options.execution = &execution;
    options.panic = &panic;
    options.trap = &observed.trap;
    options.max_steps = shared ? SIZE_MAX : fuel;
    observed.remaining = fuel;
    options.remaining_shared_steps = shared ? &observed.remaining : NULL;
    observed.result = mmay_test_vm_i64(1234);
    observed.status = hot
        ? mmay_bytecode_run_program(program, arguments, 3, allocator, options, &observed.result)
        : mmay_bytecode_run(&mmay_bytecode_program_function(program, 0)->body,
            arguments, 3, allocator, options, &observed.result);
    if (index == 0 && fuel >= 5 && !saturated) {
        MMAY_EXPECT(observed.status == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(observed.result.type == test->type);
        MMAY_EXPECT((test->type == MMAY_BYTECODE_TYPE_BOOL
            ? (observed.result.as.boolean ? 1 : 0)
            : observed.result.as.integer_bits) == test->before);
    }
    observed.panic_code = panic.code;
    observed.panic_span = panic.span;
    MMAY_EXPECT(mmay_managed_array_get(&execution.managed_heap,
        arguments[0].as.managed_reference, 0, &cell, &length) == MMAY_MANAGED_OK);
    observed.bits = cell.f64_bits;
    if (index == 0 && fuel >= 5 && !saturated) {
        MMAY_EXPECT(observed.bits == test->after);
    }
    observed.generation = *view.mutation_generation;
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_function_dispose(&builder);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    return observed;
}

static void mmay_scalar_array_compare(MarimayScalarArrayObservation cold,
    MarimayScalarArrayObservation hot) {
    MMAY_EXPECT(cold.status == hot.status);
    MMAY_EXPECT(cold.result.type == hot.result.type);
    if (cold.result.type == MMAY_BYTECODE_TYPE_BOOL) {
        MMAY_EXPECT(cold.result.as.boolean == hot.result.as.boolean);
    } else MMAY_EXPECT(cold.result.as.integer_bits == hot.result.as.integer_bits);
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


/* Assert actual region coverage, not merely equality with cold fallback. */
static void mmay_scalar_array_region_coverage(const MarimayBytecodeProgram *program,
    const MarimayScalarArrayCase *test) {
    MarimayFailAllocator state;
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeFunction builder = {0};
    MarimayBytecodeValue registers[6] = {{0}};
    MarimayManagedRootFrame roots = {0};
    const MarimayBytecodeFunction *function = &mmay_bytecode_program_function(program, 0)->body;
    const MarimayBytecodeType *types = function->register_types.data;
    for (size_t reg = 0; reg < 6; reg += 1) registers[reg].type = types[reg];
    registers[5].as.managed_reference = mmay_managed_invalid_reference();
    registers[1].as.i64 = 0;
    if (test->type == MMAY_BYTECODE_TYPE_BOOL) registers[2].as.boolean = test->after != 0;
    else registers[2].as.integer_bits = test->after;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_fail_allocator(&state),
        mmay_bytecode_execution_default_options()));
    MMAY_EXPECT(mmay_scalar_array_new(&execution, test, &registers[0], &builder));
    MarimayManagedReference *locations[] = {
        &registers[0].as.managed_reference, &registers[5].as.managed_reference
    };
    MMAY_EXPECT(mmay_managed_root_push(&execution.managed_heap, &roots, locations, 2)
        == MMAY_MANAGED_OK);
    size_t block = 0, instruction = 0, steps = 0, remaining = 5;
    size_t attempts = state.attempts;
    state.failure_index = attempts;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.max_steps = 5;
    options.remaining_shared_steps = &remaining;
    mmay_bytecode_run_hot(function, registers, &execution, &block, &instruction, &steps, options);
    MMAY_EXPECT(block == 0 && instruction == 4 && steps == 4 && remaining == 1);
    MMAY_EXPECT(state.attempts == attempts);
    MMAY_EXPECT((test->type == MMAY_BYTECODE_TYPE_BOOL
        ? (registers[4].as.boolean ? 1 : 0) : registers[4].as.integer_bits) == test->before);
    MarimayManagedCell cell = {0};
    size_t length = 0;
    MMAY_EXPECT(mmay_managed_array_get(&execution.managed_heap,
        registers[0].as.managed_reference, 0, &cell, &length) == MMAY_MANAGED_OK
        && cell.f64_bits == test->after);
    MMAY_EXPECT(mmay_managed_root_pop(&execution.managed_heap, &roots) == MMAY_MANAGED_OK);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_function_dispose(&builder);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

void mmay_test_vm_scalar_array_regions(void) {
    static const MarimayScalarArrayCase cases[] = {
        {MMAY_BYTECODE_TYPE_BOOL, 1, 0},
        {MMAY_BYTECODE_TYPE_BOOL, 0, 1},
        {MMAY_BYTECODE_TYPE_I8, UINT8_MAX, 0x80},
        {MMAY_BYTECODE_TYPE_U8, 0, UINT8_MAX},
        {MMAY_BYTECODE_TYPE_I16, UINT16_MAX, 0x8000},
        {MMAY_BYTECODE_TYPE_U16, 0, UINT16_MAX},
        {MMAY_BYTECODE_TYPE_I32, UINT32_MAX, UINT64_C(0x80000000)},
        {MMAY_BYTECODE_TYPE_U32, 0, UINT32_MAX},
        {MMAY_BYTECODE_TYPE_RUNE, 0x10ffff, 0x1f600},
        {MMAY_BYTECODE_TYPE_F32, 0x80000000, 0x7fc01234},
        {MMAY_BYTECODE_TYPE_I64, UINT64_MAX, UINT64_C(0x8000000000000000)},
        {MMAY_BYTECODE_TYPE_U64, 0, UINT64_MAX},
        {MMAY_BYTECODE_TYPE_F64, UINT64_C(0x8000000000000000), UINT64_C(0x7ff8000000000042)}
    };
    for (size_t test = 0; test < sizeof(cases) / sizeof(cases[0]); test += 1) {
        for (unsigned proven = 0; proven < 2; proven += 1) {
            MarimayBytecodeProgram program = {0};
            bool built = mmay_scalar_array_program(&program, &cases[test], proven != 0);
            MMAY_EXPECT(built);
            if (!built) { mmay_bytecode_program_dispose(&program); continue; }
            mmay_scalar_array_region_coverage(&program, &cases[test]);
            for (int64_t index = -1; index <= 1; index += 1) {
                for (size_t fuel = 1; fuel <= 6; fuel += 1) {
                    for (unsigned flags = 0; flags < 4; flags += 1) {
                        mmay_scalar_array_compare(
                            mmay_scalar_array_observe(&program, &cases[test], false,
                                index, fuel, (flags & 1) != 0, (flags & 2) != 0),
                            mmay_scalar_array_observe(&program, &cases[test], true,
                                index, fuel, (flags & 1) != 0, (flags & 2) != 0));
                    }
                }
            }
            mmay_bytecode_program_dispose(&program);
        }
    }
    static const char source[] =
        "fn pressure():void{const make=():[]bool=>[true,false];const _=make();}"
        "fn mutate(a:[]bool):void{a[0]=!a[0];const _=push(a,true);}"
        "fn main():int{let a=[true,false];const alias=a;"
        "const b:[]i8=[-128,127];const c:[]u16=[65535,0];"
        "const d:[]i32=[-2147483648,2147483647];let sum=0;"
        "for i in 0..9{if a[0]{sum+=1;}mutate(alias);pressure();"
        "b[1]=b[0];c[1]=c[0];d[1]=d[0];"
        "if b[1]!=-128||c[1]!=65535||d[1]!=-2147483648{panic(\"scalar array\");}}"
        "const old=a;a=[true];pressure();if old[0]{panic(\"alias\");}"
        "return sum+old.length()+a.length();}";
    mmay_test_callable_engines_prepared(source, 17, 1, 1);
}
