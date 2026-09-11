#include "vm_jit_internal.h"
#include "vm_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "JIT enum construction: %s at %d\n", #c, __LINE__); exit(1); } } while (0)

typedef struct Fixture { MarimayBytecodeProgram program; MarimayBytecodeType type; uint64_t bits; size_t variant; } Fixture;
static void append(MarimayBytecodeFunction *f, uint32_t block, MarimayOpcode op,
    uint64_t a, uint64_t b, uint64_t c) {
    MarimayBytecodeInstruction i = {.opcode=op,
        .span={.start=f->instruction_count+100,.end=f->instruction_count+101}};
    i.operands[0].unsigned_value=a;i.operands[1].unsigned_value=b;i.operands[2].unsigned_value=c;
    i.operands[2].signed_value=(int64_t)c;
    EXPECT(mmay_bytecode_append_instruction(f,block,&i));
}
static const MarimayBytecodeFunction *body(const Fixture *f) {
    return &mmay_bytecode_program_function(&f->program,0)->body;
}
static void fixture_init(Fixture *fixture, MarimayBytecodeType type, uint64_t bits, size_t variant) {
    *fixture=(Fixture){.type=type,.bits=bits,.variant=variant};
    MarimayAllocator a=mmay_system_allocator();MarimayBytecodeFunction f={0};uint32_t id;
    EXPECT(mmay_bytecode_program_init(&fixture->program,a,1,mmay_bytecode_program_default_options()));
    EXPECT(mmay_bytecode_function_init(&f,a,type,1,&type,1,mmay_bytecode_function_default_options()));
    MarimayBytecodeEnumVariant variants[]={{0,1},{1,0}};
    MarimayBytecodeEnumPayload payload={type,MMAY_BYTECODE_NO_LAYOUT};
    MarimaySymbolId nominal={{0x81}},symbol={{0x82}};
    EXPECT(mmay_bytecode_add_enum_layout(&f,nominal,variants,2,&payload,1,&id)&&id==0);
    EXPECT(mmay_bytecode_add_register(&f,MMAY_BYTECODE_TYPE_ENUM,&id)&&id==1);
    EXPECT(mmay_bytecode_set_register_layout(&f,1,0));
    EXPECT(mmay_bytecode_add_register(&f,MMAY_BYTECODE_TYPE_BOOL,&id)&&id==2);
    EXPECT(mmay_bytecode_add_register(&f,type,&id)&&id==3);
    MarimayBytecodeEnumArgument argument={0,type,MMAY_BYTECODE_NO_LAYOUT};
    EXPECT(mmay_bytecode_add_enum_construction(&f,0,variant,variant==0?&argument:NULL,variant==0?1:0,&id)&&id==0);
    EXPECT(mmay_bytecode_add_block(&f,&id)&&id==0);
    EXPECT(mmay_bytecode_add_block(&f,&id)&&id==1);
    append(&f,0,MMAY_OPCODE_CONST_BOOL,2,0,0);
    append(&f,0,MMAY_OPCODE_BRANCH_BOOL,2,0,1);
    append(&f,1,MMAY_OPCODE_ENUM_NEW,1,0,0);
    append(&f,1,MMAY_OPCODE_ENUM_NEW,1,0,0);
    append(&f,1,MMAY_OPCODE_ENUM_REQUIRE_VARIANT,1,1,variant);
    if(variant==0)append(&f,1,MMAY_OPCODE_ENUM_GET_PAYLOAD,3,1,0);
    append(&f,1,MMAY_OPCODE_RETURN,variant==0?3:0,0,0);
    EXPECT(mmay_bytecode_program_take_function(&fixture->program,symbol,&f,&id));
    EXPECT(mmay_bytecode_program_set_entry(&fixture->program,symbol));
    EXPECT(mmay_bytecode_program_verify(&fixture->program)==MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}
static MarimayBytecodeValue argument(const Fixture *f) {
    MarimayBytecodeValue v={.type=f->type,.as.integer_bits=f->bits};
    if(f->type==MMAY_BYTECODE_TYPE_BOOL)v.as.boolean=f->bits!=0;
    return v;
}
typedef struct Observation {
    MarimayBytecodeRunStatus status;MarimayBytecodeTrap trap;
    MarimayBytecodeValue result;size_t remaining,attempts;
} Observation;
static Observation run(const Fixture *f,bool jit,size_t fuel,bool shared,size_t fail) {
    MarimayFailAllocator failure;mmay_fail_allocator_init(&failure,mmay_system_allocator(),SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failure);MarimayBytecodeExecution e={0};
    MarimayBytecodeExecutionOptions memory=mmay_bytecode_execution_default_options();
    memory.managed_heap.collect_before_allocation=true;
    EXPECT(mmay_bytecode_execution_init(&e,a,memory));
    Observation o={.remaining=fuel,.result={.type=MMAY_BYTECODE_TYPE_I64,.as.i64=1234}};
    MarimayBytecodeRunOptions options=mmay_bytecode_run_default_options();
    options.execution=&e;options.trap=&o.trap;options.max_steps=shared?SIZE_MAX:fuel;
    options.remaining_shared_steps=shared?&o.remaining:NULL;
    MarimayBytecodeValue arg=argument(f);failure.attempts=0;failure.failure_index=fail;
    o.status=jit?mmay_bytecode_run_program(&f->program,&arg,1,a,options,&o.result)
        :mmay_bytecode_run(body(f),&arg,1,a,options,&o.result);
    o.attempts=failure.attempts;
    if(fail<failure.attempts)EXPECT(o.status==MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
    EXPECT(e.managed_heap.roots==NULL);mmay_bytecode_execution_dispose(&e);
    EXPECT(failure.live_allocations==0&&failure.live_bytes==0);return o;
}
static void compare(const Fixture *f,size_t fuel,bool shared) {
    Observation a=run(f,false,fuel,shared,SIZE_MAX),b=run(f,true,fuel,shared,SIZE_MAX);
    EXPECT(a.status==b.status&&a.remaining==b.remaining&&a.result.type==b.result.type);
    EXPECT(a.result.as.integer_bits==b.result.as.integer_bits);
    EXPECT(a.trap.has_span==b.trap.has_span);
    if(a.trap.has_span)EXPECT(a.trap.span.start==b.trap.span.start&&a.trap.span.end==b.trap.span.end);
    if(a.status==MMAY_BYTECODE_RUN_OK) {
        MarimayBytecodeValue expected=argument(f);EXPECT(a.result.as.integer_bits==expected.as.integer_bits);
    }
}
/* Warm 0 lacks a descriptor; warm 1 has a descriptor but no runtime ID; warm 2
 * has both. The same JIT also sees a disposed/reinitialized execution address. */
static void native(const Fixture *f,size_t warm) {
    MarimayFailAllocator failure;mmay_fail_allocator_init(&failure,mmay_system_allocator(),SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failure);MarimayBytecodeExecutionImage image={0};MarimayBytecodeJit jit={0};
    MarimayCodeMemoryFault pages={.failure_index=SIZE_MAX};MarimayBytecodeExecution e={0};
    EXPECT(mmay_bytecode_execution_image_init(&image,&f->program,a)==MMAY_BYTECODE_IMAGE_OK);
    EXPECT(mmay_bytecode_jit_init(&jit,&f->program,&image,a,&pages,true)==MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready&&jit.functions[0].plan[2].supported);
    uint64_t previous_provenance=0;
    for(size_t round=0;round<2;++round) {
        failure.failure_index=SIZE_MAX;
        EXPECT(mmay_bytecode_execution_init(&e,a,mmay_bytecode_execution_default_options()));
        EXPECT(e.managed_heap.provenance!=previous_provenance);previous_provenance=e.managed_heap.provenance;
        MarimayBytecodeValue regs[4]={argument(f),{.type=MMAY_BYTECODE_TYPE_ENUM},{.type=MMAY_BYTECODE_TYPE_BOOL},{.type=f->type}};
        regs[1].as.managed_reference=mmay_managed_invalid_reference();
        const void *descriptor=NULL;
        if(warm>0)EXPECT(mmay_bytecode_run_nominal_descriptor(&e,body(f),MMAY_BYTECODE_TYPE_ENUM,0,&descriptor)==MMAY_BYTECODE_RUN_OK);
        if(warm==2) {
            const MarimayBytecodeBlock *b=mmay_vector_at_const(&body(f)->blocks,1);
            const MarimayBytecodeInstruction *i=mmay_vector_at_const(&b->instructions,0);
            EXPECT(mmay_bytecode_run_nominal_instruction(i,regs,&e,body(f),a)==MMAY_BYTECODE_RUN_OK);
        }
        MarimayManagedReference before=regs[1].as.managed_reference;
        size_t block=1,pc=0,steps=0,budget=20,attempts=failure.attempts;
        size_t bytes=e.managed_heap.live_byte_count;uint64_t epoch=e.managed_heap.view_epoch;
        e.managed_heap.last_resource_failure=(MarimayManagedResourceFailure){.classification=MMAY_MANAGED_RESOURCE_FAILURE_BUDGET,.requested_bytes=77};
        e.managed_heap.options.collect_before_allocation=true;failure.failure_index=attempts;
        MarimayBytecodeRunOptions options=mmay_bytecode_run_default_options();
        options.execution=&e;options.max_steps=SIZE_MAX;options.remaining_shared_steps=&budget;
        EXPECT(mmay_bytecode_jit_run(&jit,0,&image.functions[0],body(f),regs,&e,&block,&pc,&steps,options));
        EXPECT(failure.attempts==attempts&&e.managed_heap.live_byte_count==bytes&&e.managed_heap.view_epoch==epoch);
        EXPECT(block==1&&pc==(warm==2?(f->variant==0?4:3):0));
        EXPECT(budget==(warm==2?(f->variant==0?16:17):20));
        if(warm<2) {
            EXPECT(mmay_managed_reference_equal(before,regs[1].as.managed_reference));
            EXPECT(e.managed_heap.last_resource_failure.requested_bytes==77);
        } else {
            const MarimayManagedEnumDescriptor *actual=NULL;size_t variant=2;
            EXPECT(mmay_managed_enum_describe(&e.managed_heap,regs[1].as.managed_reference,&actual,&variant)==MMAY_MANAGED_OK);
            EXPECT(actual==descriptor&&variant==f->variant);
            EXPECT(e.managed_heap.last_resource_failure.classification==MMAY_MANAGED_RESOURCE_FAILURE_NONE);
        }
        mmay_bytecode_execution_dispose(&e);
    }
    mmay_bytecode_jit_dispose(&jit);mmay_bytecode_execution_image_dispose(&image);
    EXPECT(failure.live_allocations==0&&failure.live_bytes==0);
    EXPECT(pages.live_mappings==0&&pages.live_bytes==0);
}
static void admission(void) {
    MarimayBytecodeType cold_types[]={MMAY_BYTECODE_TYPE_STRING,MMAY_BYTECODE_TYPE_F32};
    for(size_t t=0;t<sizeof(cold_types)/sizeof(*cold_types);++t)for(size_t variant=0;variant<2;++variant) {
        Fixture f;fixture_init(&f,cold_types[t],0,variant);
        MarimayBytecodeExecutionImage image={0};MarimayBytecodeJit jit={0};
        MarimayFailAllocator failure;mmay_fail_allocator_init(&failure,mmay_system_allocator(),SIZE_MAX);
        MarimayAllocator a=mmay_fail_allocator(&failure);MarimayCodeMemoryFault pages={.failure_index=SIZE_MAX};
        EXPECT(mmay_bytecode_execution_image_init(&image,&f.program,a)==MMAY_BYTECODE_IMAGE_OK);
        EXPECT(mmay_bytecode_jit_init(&jit,&f.program,&image,a,&pages,true)==MMAY_BYTECODE_RUN_OK);
        EXPECT(jit.ready&&jit.functions[0].plan[2].supported==(variant==1));
        EXPECT(!mmay_bytecode_jit_enum_new_supported(NULL,&image.functions[0].operations[2]));
        EXPECT(!mmay_bytecode_jit_enum_new_supported(body(&f),NULL));
        EXPECT(!mmay_bytecode_jit_enum_new_supported(body(&f),&image.functions[0].operations[0]));
        mmay_bytecode_jit_dispose(&jit);mmay_bytecode_execution_image_dispose(&image);
        mmay_bytecode_program_dispose(&f.program);
        EXPECT(failure.live_allocations==0&&failure.live_bytes==0);
        EXPECT(pages.live_mappings==0&&pages.live_bytes==0);
    }
}
int main(void) {
    if(!mmay_code_memory_supported())return 77;
    admission();
    const struct {MarimayBytecodeType type;uint64_t bits;} cases[]={
        {MMAY_BYTECODE_TYPE_BOOL,1},{MMAY_BYTECODE_TYPE_I8,255},{MMAY_BYTECODE_TYPE_U8,255},
        {MMAY_BYTECODE_TYPE_I16,65535},{MMAY_BYTECODE_TYPE_U16,65535},
        {MMAY_BYTECODE_TYPE_I32,UINT32_MAX},{MMAY_BYTECODE_TYPE_U32,UINT32_MAX},
        {MMAY_BYTECODE_TYPE_I64,UINT64_MAX},{MMAY_BYTECODE_TYPE_U64,UINT64_MAX},
        {MMAY_BYTECODE_TYPE_RUNE,0x10ffff},
        {MMAY_BYTECODE_TYPE_F64,UINT64_C(0x8000000000000000)}};
    for(size_t t=0;t<sizeof(cases)/sizeof(*cases);++t)for(size_t variant=0;variant<2;++variant) {
        Fixture f;fixture_init(&f,cases[t].type,cases[t].bits,variant);
        for(size_t warm=0;warm<3;++warm)native(&f,warm);
        for(size_t shared=0;shared<2;++shared) {
            compare(&f,SIZE_MAX,shared!=0);
            for(size_t fuel=1;fuel<=9;++fuel)compare(&f,fuel,shared!=0);
        }
        for(size_t metered=0;metered<2;++metered) {
            size_t fuel=metered?20:SIZE_MAX,count=run(&f,true,fuel,false,SIZE_MAX).attempts;
            for(size_t i=0;i<=count;++i)run(&f,true,fuel,false,i);
        }
        mmay_bytecode_program_dispose(&f.program);
    }
    puts("JIT enum construction: cached nonallocating native entry, cold descriptor/ID misses, heap lifetime, scalar bits, exact fuel and allocation failures: passed");
    return 0;
}
