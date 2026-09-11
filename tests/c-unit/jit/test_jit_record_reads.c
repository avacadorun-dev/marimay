#include "vm_jit_record_reads.h"
#include "vm_jit_internal.h"
#include "vm_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(c) do { if (!(c)) { fprintf(stderr,"JIT record reuse: %s at %d\n",#c,__LINE__); exit(1); } } while (0)

static void expressions(void) {
    MarimayBytecodeType types[]={MMAY_BYTECODE_TYPE_RECORD,MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,MMAY_BYTECODE_TYPE_RECORD,MMAY_BYTECODE_TYPE_RECORD};
    uint32_t layouts[]={0,MMAY_BYTECODE_NO_LAYOUT,MMAY_BYTECODE_NO_LAYOUT,1,2};
    MarimayBytecodeFunction f={.register_types={.data=types,.length=5},
        .register_layouts={.data=layouts,.length=5}};
    MarimayBytecodeJitRecordRead storage[5]; MarimayBytecodeJitRecordReads reads;
    mmay_bytecode_jit_record_reads_init(&reads,&f,storage);
    mmay_bytecode_jit_record_read_remember(&reads,0,UINT64_C(0x100000001),1);
    EXPECT(mmay_bytecode_jit_record_read_find(&reads,0,UINT64_C(0x100000001),2)==1);
    EXPECT(mmay_bytecode_jit_record_read_find(&reads,0,1,2)==UINT32_MAX);
    EXPECT(mmay_bytecode_jit_record_read_find(&reads,3,UINT64_C(0x100000001),2)==UINT32_MAX);
    mmay_bytecode_jit_record_read_write(&reads,1);
    EXPECT(mmay_bytecode_jit_record_read_find(&reads,0,UINT64_C(0x100000001),2)==UINT32_MAX);
    mmay_bytecode_jit_record_read_remember(&reads,0,0,3);
    EXPECT(mmay_bytecode_jit_record_read_find(&reads,0,0,4)==UINT32_MAX);
    mmay_bytecode_jit_record_read_remember(&reads,0,0,1);
    mmay_bytecode_jit_record_read_write(&reads,0);
    EXPECT(mmay_bytecode_jit_record_read_find(&reads,0,0,2)==UINT32_MAX);
    mmay_bytecode_jit_record_read_remember(&reads,0,0,0);
    EXPECT(mmay_bytecode_jit_record_read_find(&reads,0,0,0)==UINT32_MAX);
    mmay_bytecode_jit_record_read_remember(&reads,0,0,1);
    mmay_bytecode_jit_record_reads_reset(&reads);
    EXPECT(mmay_bytecode_jit_record_read_find(&reads,0,0,2)==UINT32_MAX);
}

static void append(MarimayBytecodeFunction *f,uint32_t block,MarimayOpcode opcode,
    uint32_t a,uint32_t b,uint32_t c) {
    MarimayBytecodeInstruction op={.opcode=opcode,
        .span={.start=f->instruction_count+100,.end=f->instruction_count+101}};
    op.operands[0].unsigned_value=a; op.operands[1].unsigned_value=b;
    op.operands[2].unsigned_value=c;
    EXPECT(mmay_bytecode_append_instruction(f,block,&op));
}

static void fixture(MarimayBytecodeProgram *program,bool mutate) {
    MarimayAllocator a=mmay_system_allocator(); MarimayBytecodeFunction f={0}; uint32_t id;
    MarimayBytecodeType params[]={MMAY_BYTECODE_TYPE_RECORD,MMAY_BYTECODE_TYPE_ARRAY_F64};
    MarimayBytecodeType locals[]={MMAY_BYTECODE_TYPE_ARRAY_F64,MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64,MMAY_BYTECODE_TYPE_I64,MMAY_BYTECODE_TYPE_I64,MMAY_BYTECODE_TYPE_BOOL};
    EXPECT(mmay_bytecode_program_init(program,a,1,mmay_bytecode_program_default_options()));
    EXPECT(mmay_bytecode_function_init(&f,a,MMAY_BYTECODE_TYPE_I64,1,params,2,
        mmay_bytecode_function_default_options()));
    for(size_t i=0;i<6;++i) EXPECT(mmay_bytecode_add_register(&f,locals[i],&id));
    MarimayBytecodeRecordField field={.type=MMAY_BYTECODE_TYPE_ARRAY_F64,
        .layout=MMAY_BYTECODE_NO_LAYOUT,.is_mutable=true};
    EXPECT(mmay_bytecode_add_record_layout(&f,(MarimaySymbolId){{0x61}},&field,1,&id));
    EXPECT(id==0 && mmay_bytecode_set_register_layout(&f,0,0));
    EXPECT(mmay_bytecode_add_block(&f,&id) && mmay_bytecode_add_block(&f,&id));
    append(&f,0,MMAY_OPCODE_CONST_BOOL,7,0,0); append(&f,0,MMAY_OPCODE_BRANCH_BOOL,7,0,1);
    append(&f,1,MMAY_OPCODE_RECORD_GET,2,0,0); append(&f,1,MMAY_OPCODE_ARRAY_LENGTH,4,2,0);
    if(mutate) append(&f,1,MMAY_OPCODE_RECORD_SET,1,0,0);
    append(&f,1,MMAY_OPCODE_RECORD_GET,3,0,0); append(&f,1,MMAY_OPCODE_ARRAY_LENGTH,5,3,0);
    append(&f,1,MMAY_OPCODE_I64_ADD_CHECKED,6,4,5); append(&f,1,MMAY_OPCODE_RETURN,6,0,0);
    MarimaySymbolId symbol={{0x62}};
    EXPECT(mmay_bytecode_program_take_function(program,symbol,&f,&id));
    EXPECT(mmay_bytecode_program_set_entry(program,symbol));
    EXPECT(mmay_bytecode_program_verify(program)==MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static void objects(const MarimayBytecodeFunction *f,MarimayBytecodeExecution *execution,
    MarimayAllocator a,MarimayBytecodeValue args[2]) {
    EXPECT(mmay_bytecode_execution_init(execution,a,mmay_bytecode_execution_default_options()));
    MarimayManagedReference refs[3];
    for(size_t i=0;i<3;++i) refs[i]=mmay_managed_invalid_reference();
    MarimayManagedReference *locations[]={&refs[0],&refs[1],&refs[2]}; MarimayManagedRootFrame roots={0};
    EXPECT(mmay_managed_root_push(&execution->managed_heap,&roots,locations,3)==MMAY_MANAGED_OK);
    MarimayManagedCell cells[5]={{0}};
    for(size_t i=0;i<2;++i) EXPECT(mmay_managed_array_new(&execution->managed_heap,
        &mmay_managed_array_f64_descriptor,cells,3+2*i,&refs[i])==MMAY_MANAGED_OK);
    const void *descriptor=mmay_bytecode_nominal_reference_descriptor(execution,f,MMAY_BYTECODE_TYPE_RECORD,0);
    MarimayManagedCell field={.reference=refs[0]};
    EXPECT(descriptor!=NULL && mmay_managed_record_new(&execution->managed_heap,descriptor,&field,1,&refs[2])==MMAY_MANAGED_OK);
    args[0]=(MarimayBytecodeValue){.type=MMAY_BYTECODE_TYPE_RECORD,.as.managed_reference=refs[2]};
    args[1]=(MarimayBytecodeValue){.type=MMAY_BYTECODE_TYPE_ARRAY_F64,.as.managed_reference=refs[1]};
    EXPECT(mmay_managed_root_pop(&execution->managed_heap,&roots)==MMAY_MANAGED_OK);
}

typedef struct Observation {
    MarimayBytecodeRunStatus status;
    MarimayBytecodeTrap trap;
    MarimayBytecodeValue result;
    size_t remaining,attempts;
} Observation;

static Observation run(const MarimayBytecodeProgram *p,bool native,size_t frame,
    size_t shared,bool has_shared,size_t failure) {
    MarimayFailAllocator failing; mmay_fail_allocator_init(&failing,mmay_system_allocator(),SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failing); MarimayBytecodeExecution execution={0};
    const MarimayBytecodeFunction *f=&mmay_bytecode_program_function(p,0)->body;
    MarimayBytecodeValue args[2]; objects(f,&execution,a,args);
    Observation o={.remaining=shared,.result={.type=MMAY_BYTECODE_TYPE_I64,.as.i64=1234}};
    MarimayBytecodeRunOptions options=mmay_bytecode_run_default_options();
    options.execution=&execution; options.trap=&o.trap; options.max_steps=frame;
    options.remaining_shared_steps=has_shared?&o.remaining:NULL;
    failing.attempts=0; failing.failure_index=failure;
    o.status=native?mmay_bytecode_run_program(p,args,2,a,options,&o.result)
        :mmay_bytecode_run(f,args,2,a,options,&o.result);
    o.attempts=failing.attempts;
    if(failure<o.attempts) EXPECT(o.status==MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
    EXPECT(execution.managed_heap.roots==NULL); mmay_bytecode_execution_dispose(&execution);
    EXPECT(failing.live_allocations==0 && failing.live_bytes==0);
    return o;
}

static void compare(const MarimayBytecodeProgram *p,bool mutate,size_t frame,
    size_t shared,bool has_shared) {
    Observation a=run(p,false,frame,shared,has_shared,SIZE_MAX);
    Observation b=run(p,true,frame,shared,has_shared,SIZE_MAX);
    EXPECT(a.status==b.status && a.result.type==b.result.type && a.result.as.i64==b.result.as.i64);
    EXPECT(a.remaining==b.remaining && a.trap.has_span==b.trap.has_span);
    if(a.trap.has_span) EXPECT(a.trap.span.start==b.trap.span.start && a.trap.span.end==b.trap.span.end);
    if(a.status==MMAY_BYTECODE_RUN_OK) EXPECT(a.result.as.i64==(mutate?8:6));
}

static void native_proof(void) {
    MarimayBytecodeProgram program={0}; fixture(&program,false);
    MarimayFailAllocator failing; mmay_fail_allocator_init(&failing,mmay_system_allocator(),SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failing); MarimayBytecodeExecution execution={0};
    const MarimayBytecodeFunction *f=&mmay_bytecode_program_function(&program,0)->body;
    MarimayBytecodeValue args[2]; objects(f,&execution,a,args);
    MarimayBytecodeExecutionImage image={0}; MarimayBytecodeJit jit={0};
    MarimayCodeMemoryFault pages={.failure_index=SIZE_MAX};
    EXPECT(mmay_bytecode_execution_image_init(&image,&program,a)==MMAY_BYTECODE_IMAGE_OK);
    EXPECT(mmay_bytecode_jit_init(&jit,&program,&image,a,&pages,true)==MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready);
    const MarimayBytecodeType *types=f->register_types.data; MarimayBytecodeValue regs[8]={0};
    for(size_t i=0;i<8;++i) {
        regs[i].type=types[i];
        if(mmay_bytecode_type_is_managed(types[i])) regs[i].as.managed_reference=mmay_managed_invalid_reference();
    }
    memcpy(regs,args,sizeof(args));
    MarimayManagedReference *locations[]={&regs[0].as.managed_reference,&regs[1].as.managed_reference,
        &regs[2].as.managed_reference,&regs[3].as.managed_reference}; MarimayManagedRootFrame roots={0};
    EXPECT(mmay_managed_root_push(&execution.managed_heap,&roots,locations,4)==MMAY_MANAGED_OK);
    size_t block=0,pc=0,steps=0; MarimayBytecodeRunOptions options=mmay_bytecode_run_default_options();
    options.max_steps=SIZE_MAX; options.execution=&execution;
    EXPECT(mmay_bytecode_jit_run(&jit,0,&image.functions[0],f,regs,&execution,&block,&pc,&steps,options));
    EXPECT(block==1 && pc==5 && regs[6].as.i64==6);
    MarimayBytecodeJitBinding *bindings=jit.bindings;
    EXPECT(bindings[2].epoch==jit.epoch && bindings[3].epoch==0);
    EXPECT(mmay_managed_reference_equal(regs[2].as.managed_reference,regs[3].as.managed_reference));
    EXPECT(mmay_managed_root_pop(&execution.managed_heap,&roots)==MMAY_MANAGED_OK);
    mmay_bytecode_jit_dispose(&jit); mmay_bytecode_execution_image_dispose(&image);
    mmay_bytecode_execution_dispose(&execution); mmay_bytecode_program_dispose(&program);
    EXPECT(failing.live_allocations==0 && failing.live_bytes==0);
    EXPECT(pages.live_mappings==0 && pages.live_bytes==0);
}

int main(void) {
    expressions();
    if(!mmay_code_memory_supported()) return 77;
    native_proof();
    for(size_t mutate=0;mutate<2;++mutate) {
        MarimayBytecodeProgram program={0}; fixture(&program,mutate!=0);
        for(size_t fuel=0;fuel<=14;++fuel) {
            compare(&program,mutate!=0,fuel,0,false);
            compare(&program,mutate!=0,SIZE_MAX,fuel,true);
            compare(&program,mutate!=0,fuel,fuel+1,true);
            compare(&program,mutate!=0,fuel+1,fuel,true);
        }
        compare(&program,mutate!=0,SIZE_MAX,SIZE_MAX,true);
        compare(&program,mutate!=0,SIZE_MAX,0,false);
        Observation pass=run(&program,true,SIZE_MAX,0,false,SIZE_MAX);
        for(size_t failure=0;failure<=pass.attempts;++failure)
            (void)run(&program,true,SIZE_MAX,0,false,failure);
        mmay_bytecode_program_dispose(&program);
    }
    puts("JIT record reads: exact keys/layouts, overwritten receivers/results, native reuse, cold mutation, fuel and failure cleanup passed");
    return 0;
}
