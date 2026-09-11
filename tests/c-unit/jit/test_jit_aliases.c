#include "vm_jit_aliases.h"
#include "bytecode_internal.h"
#include "vm_jit_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "JIT aliases: %s at %d\n", #c, __LINE__); exit(1); } } while (0)

static void relations(void) {
    MarimayBytecodeType types[] = {MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64, MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_RECORD, MMAY_BYTECODE_TYPE_RECORD, MMAY_BYTECODE_TYPE_I64};
    uint32_t layouts[] = {MMAY_BYTECODE_NO_LAYOUT, MMAY_BYTECODE_NO_LAYOUT,
        MMAY_BYTECODE_NO_LAYOUT, 0, 1, MMAY_BYTECODE_NO_LAYOUT};
    MarimayBytecodeFunction f = {.register_types={.data=types, .length=6},
        .register_layouts={.data=layouts, .length=6}};
    uint32_t storage[6]; MarimayBytecodeJitAliases aliases;
    mmay_bytecode_jit_aliases_init(&aliases, &f, storage);
    /* All sequences of six writes/copies over three registers. Every relation
     * must describe actual value equality after canonical-register overwrite. */
    for (size_t sequence = 0; sequence < 15625; ++sequence) {
        mmay_bytecode_jit_aliases_reset(&aliases);
        size_t code = sequence, values[] = {0, 1, 2};
        for (size_t step = 0; step < 6; ++step) {
            size_t op = code % 5; code /= 5;
            if (op < 3) {
                uint32_t destination = (uint32_t)((op + 1) % 3), source = (uint32_t)op;
                values[destination] = values[source];
                mmay_bytecode_jit_alias_copy(&aliases, destination, source);
            } else {
                uint32_t destination = (uint32_t)(op - 3);
                values[destination] = step + 10;
                mmay_bytecode_jit_alias_write(&aliases, destination);
            }
            for (uint32_t reg = 0; reg < 3; ++reg) {
                uint32_t canonical = mmay_bytecode_jit_alias_source(&aliases, reg);
                EXPECT(canonical < 3 && values[reg] == values[canonical]);
                EXPECT(mmay_bytecode_jit_alias_source(&aliases, canonical) == canonical);
            }
        }
    }
    mmay_bytecode_jit_aliases_reset(&aliases);
    mmay_bytecode_jit_alias_copy(&aliases, 1, 0);
    mmay_bytecode_jit_alias_copy(&aliases, 2, 1);
    EXPECT(mmay_bytecode_jit_alias_source(&aliases, 2) == 0);
    mmay_bytecode_jit_alias_copy(&aliases, 4, 3);
    EXPECT(mmay_bytecode_jit_alias_source(&aliases, 4) == 4);
    mmay_bytecode_jit_alias_copy(&aliases, 5, 5);
    EXPECT(mmay_bytecode_jit_alias_source(&aliases, 5) == 5);
    mmay_bytecode_jit_aliases_reset(&aliases);
    for (uint32_t reg = 0; reg < 6; ++reg) EXPECT(mmay_bytecode_jit_alias_source(&aliases, reg) == reg);
}

static void append(MarimayBytecodeFunction *f, uint32_t block, MarimayOpcode opcode,
    uint32_t a, uint32_t b, uint32_t c) {
    MarimayBytecodeInstruction op = {.opcode=opcode,
        .span={.start=f->instruction_count + 100, .end=f->instruction_count + 101}};
    op.operands[0].unsigned_value=a; op.operands[1].unsigned_value=b;
    op.operands[2].unsigned_value=c;
    EXPECT(mmay_bytecode_append_instruction(f, block, &op));
}

static void fixture(MarimayBytecodeProgram *program, bool short_run, bool proven) {
    MarimayAllocator a=mmay_system_allocator();
    MarimayBytecodeType parameters[]={MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64, MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_I64};
    MarimayBytecodeType locals[]={MMAY_BYTECODE_TYPE_ARRAY_F64, MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_BOOL};
    MarimayBytecodeFunction f={0}; uint32_t id;
    EXPECT(mmay_bytecode_program_init(program,a,1,mmay_bytecode_program_default_options()));
    EXPECT(mmay_bytecode_function_init(&f,a,MMAY_BYTECODE_TYPE_I64,1,parameters,4,
        mmay_bytecode_function_default_options()));
    for (size_t i=0;i<6;++i) EXPECT(mmay_bytecode_add_register(&f,locals[i],&id));
    for (size_t i=0;i<(short_run?2U:4U);++i) EXPECT(mmay_bytecode_add_block(&f,&id));
    append(&f,0,MMAY_OPCODE_CONST_BOOL,9,0,0);
    append(&f,0,MMAY_OPCODE_BRANCH_BOOL,9,0,1);
    append(&f,1,proven?MMAY_OPCODE_ARRAY_REQUIRE_INDEX:MMAY_OPCODE_COPY,4,0,3);
    append(&f,1,MMAY_OPCODE_COPY,5,4,0);
    append(&f,1,MMAY_OPCODE_ARRAY_LENGTH,6,5,0);
    if (short_run) append(&f,1,MMAY_OPCODE_RETURN,6,0,0);
    else {
        append(&f,1,MMAY_OPCODE_COPY,0,1,0);
        append(&f,1,MMAY_OPCODE_ARRAY_LENGTH,7,5,0);
        append(&f,1,MMAY_OPCODE_ARRAY_LENGTH,8,0,0);
        append(&f,1,MMAY_OPCODE_I64_ADD_CHECKED,6,6,7);
        append(&f,1,MMAY_OPCODE_I64_ADD_CHECKED,6,6,8);
        append(&f,1,MMAY_OPCODE_BRANCH_BOOL,2,2,3);
        append(&f,2,MMAY_OPCODE_COPY,4,1,0);
        append(&f,2,MMAY_OPCODE_JUMP,3,0,0);
        append(&f,3,MMAY_OPCODE_ARRAY_LENGTH,7,4,0);
        append(&f,3,MMAY_OPCODE_I64_ADD_CHECKED,6,6,7);
        append(&f,3,MMAY_OPCODE_RETURN,6,0,0);
    }
    MarimaySymbolId symbol={{0x53}};
    EXPECT(mmay_bytecode_program_take_function(program,symbol,&f,&id));
    EXPECT(mmay_bytecode_program_set_entry(program,symbol));
    EXPECT(mmay_bytecode_program_verify(program)==MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static void objects(MarimayBytecodeExecution *execution, MarimayAllocator a,
    MarimayBytecodeValue arguments[4], bool branch, int64_t index) {
    EXPECT(mmay_bytecode_execution_init(execution,a,mmay_bytecode_execution_default_options()));
    MarimayManagedCell cells[5]={{0}};
    for (size_t i=0;i<2;++i) {
        arguments[i].type=MMAY_BYTECODE_TYPE_ARRAY_F64;
        EXPECT(mmay_managed_array_new(&execution->managed_heap,&mmay_managed_array_f64_descriptor,
            cells,3+2*i,&arguments[i].as.managed_reference)==MMAY_MANAGED_OK);
    }
    arguments[2]=(MarimayBytecodeValue){.type=MMAY_BYTECODE_TYPE_BOOL,.as.boolean=branch};
    arguments[3]=(MarimayBytecodeValue){.type=MMAY_BYTECODE_TYPE_I64,.as.i64=index};
}

typedef struct Observation {
    MarimayBytecodeRunStatus status;
    MarimayBytecodeTrap trap;
    MarimayBytecodeValue result;
    size_t remaining, attempts;
} Observation;

static Observation run(const MarimayBytecodeProgram *program, bool native,
    bool branch, int64_t index, size_t frame, size_t shared, bool has_shared, size_t failure) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing,mmay_system_allocator(),SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failing);
    MarimayBytecodeExecution execution={0}; MarimayBytecodeValue args[4];
    objects(&execution,a,args,branch,index);
    Observation o={.result={.type=MMAY_BYTECODE_TYPE_I64,.as.i64=1234},.remaining=shared};
    MarimayBytecodeRunOptions options=mmay_bytecode_run_default_options();
    options.execution=&execution; options.trap=&o.trap; options.max_steps=frame;
    options.remaining_shared_steps=has_shared?&o.remaining:NULL;
    failing.attempts=0; failing.failure_index=failure;
    o.status=native?mmay_bytecode_run_program(program,args,4,a,options,&o.result)
        :mmay_bytecode_run(&mmay_bytecode_program_function(program,0)->body,args,4,a,options,&o.result);
    o.attempts=failing.attempts;
    if (failure<o.attempts) EXPECT(o.status==MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
    EXPECT(execution.managed_heap.roots==NULL);
    mmay_bytecode_execution_dispose(&execution);
    EXPECT(failing.live_allocations==0 && failing.live_bytes==0);
    return o;
}

static void compare(const MarimayBytecodeProgram *p, bool branch, int64_t index,
    size_t frame, size_t shared, bool has_shared) {
    Observation a=run(p,false,branch,index,frame,shared,has_shared,SIZE_MAX);
    Observation b=run(p,true,branch,index,frame,shared,has_shared,SIZE_MAX);
    EXPECT(a.status==b.status && a.result.type==b.result.type && a.result.as.i64==b.result.as.i64);
    EXPECT(a.remaining==b.remaining && a.trap.has_span==b.trap.has_span);
    if (a.trap.has_span) EXPECT(a.trap.span.start==b.trap.span.start && a.trap.span.end==b.trap.span.end);
    if (a.status==MMAY_BYTECODE_RUN_OK) EXPECT(a.result.as.i64==(branch?16:14));
}

static void native_proof(bool proven) {
    MarimayBytecodeProgram program={0}; fixture(&program,true,proven);
    MarimayFailAllocator failing; mmay_fail_allocator_init(&failing,mmay_system_allocator(),SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failing);
    MarimayBytecodeExecution execution={0}; MarimayBytecodeValue args[4]; objects(&execution,a,args,false,0);
    MarimayBytecodeExecutionImage image={0}; MarimayBytecodeJit jit={0};
    MarimayCodeMemoryFault pages={.failure_index=SIZE_MAX};
    EXPECT(mmay_bytecode_execution_image_init(&image,&program,a)==MMAY_BYTECODE_IMAGE_OK);
    EXPECT(mmay_bytecode_jit_init(&jit,&program,&image,a,&pages,true)==MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready);
    const MarimayBytecodeFunction *f=&mmay_bytecode_program_function(&program,0)->body;
    const MarimayBytecodeType *types=f->register_types.data; MarimayBytecodeValue regs[10]={0};
    for (size_t i=0;i<10;++i) {
        regs[i].type=types[i];
        if (mmay_bytecode_type_is_managed(types[i])) regs[i].as.managed_reference=mmay_managed_invalid_reference();
    }
    memcpy(regs,args,sizeof(args));
    MarimayManagedReference *locations[]={&regs[0].as.managed_reference,&regs[1].as.managed_reference,
        &regs[4].as.managed_reference,&regs[5].as.managed_reference};
    MarimayManagedRootFrame roots={0};
    EXPECT(mmay_managed_root_push(&execution.managed_heap,&roots,locations,4)==MMAY_MANAGED_OK);
    size_t block=0,pc=0,steps=0; MarimayBytecodeRunOptions options=mmay_bytecode_run_default_options();
    options.max_steps=SIZE_MAX; options.execution=&execution;
    EXPECT(mmay_bytecode_jit_run(&jit,0,&image.functions[0],f,regs,&execution,&block,&pc,&steps,options));
    EXPECT(block==1 && pc==3 && regs[6].as.i64==3);
    MarimayBytecodeJitBinding *bindings=jit.bindings;
    EXPECT(bindings[0].epoch==jit.epoch && bindings[4].epoch==0 && bindings[5].epoch==0);
    EXPECT(mmay_managed_reference_equal(regs[4].as.managed_reference,args[0].as.managed_reference));
    EXPECT(mmay_managed_reference_equal(regs[5].as.managed_reference,args[0].as.managed_reference));
    EXPECT(mmay_managed_root_pop(&execution.managed_heap,&roots)==MMAY_MANAGED_OK);
    mmay_bytecode_jit_dispose(&jit); mmay_bytecode_execution_image_dispose(&image);
    mmay_bytecode_execution_dispose(&execution); mmay_bytecode_program_dispose(&program);
    EXPECT(failing.live_allocations==0 && failing.live_bytes==0);
    EXPECT(pages.live_mappings==0 && pages.live_bytes==0);
}

int main(void) {
    relations();
    if (!mmay_code_memory_supported()) return 77;
    native_proof(false); native_proof(true);
    for (size_t proven=0;proven<2;++proven) {
        MarimayBytecodeProgram program={0}; fixture(&program,false,proven!=0);
        for (size_t branch=0;branch<2;++branch) {
            const int64_t indices[]={INT64_MIN,-1,0,1,2,3,INT64_MAX};
            for (size_t i=0;i<sizeof(indices)/sizeof(*indices);++i) {
                int64_t index=indices[i];
                for (size_t fuel=0;fuel<=18;++fuel) {
                    compare(&program,branch!=0,index,fuel,0,false);
                    compare(&program,branch!=0,index,SIZE_MAX,fuel,true);
                    compare(&program,branch!=0,index,fuel,fuel+1,true);
                    compare(&program,branch!=0,index,fuel+1,fuel,true);
                }
                compare(&program,branch!=0,index,SIZE_MAX,SIZE_MAX,true);
                compare(&program,branch!=0,index,SIZE_MAX,0,false);
            }
        }
        Observation pass=run(&program,true,true,0,SIZE_MAX,0,false,SIZE_MAX);
        for (size_t failure=0;failure<=pass.attempts;++failure)
            (void)run(&program,true,true,0,SIZE_MAX,0,false,failure);
        mmay_bytecode_program_dispose(&program);
    }
    puts("JIT aliases: exhaustive relation transitions, native canonical binding, overwrites/joins, exact fuel/exits and failure cleanup passed");
    return 0;
}
