#include "vm_jit_internal.h"
#include "vm_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "JIT strings: %s at %d\n", #c, __LINE__); exit(1); } } while (0)

typedef struct Fixture {
    MarimayBytecodeProgram program;
    bool proven;
    uint32_t layout;
    size_t first_read, cold;
    const unsigned char *left, *right;
    size_t left_length, right_length;
} Fixture;

static void append(MarimayBytecodeFunction *f, uint32_t block, MarimayOpcode op,
    uint64_t a, uint64_t b, uint64_t c) {
    MarimayBytecodeInstruction i = {.opcode=op,
        .span={.start=f->instruction_count+100, .end=f->instruction_count+101}};
    i.operands[0].unsigned_value=a; i.operands[1].unsigned_value=b;
    i.operands[2].unsigned_value=c;
    if (op == MMAY_OPCODE_CONST_I64) i.operands[1].signed_value=(int64_t)b;
    EXPECT(mmay_bytecode_append_instruction(f, block, &i));
}

static void read_string(Fixture *f, MarimayBytecodeFunction *body) {
    if (f->proven) append(body, 1, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 6, 0, 1);
    append(body, 1, f->proven ? MMAY_OPCODE_ARRAY_GET_IN_BOUNDS : MMAY_OPCODE_ARRAY_GET,
        2, f->proven ? 6 : 0, 1);
}

static void init(Fixture *f, bool proven, const unsigned char *left, size_t nl,
    const unsigned char *right, size_t nr) {
    *f=(Fixture){.proven=proven, .left=left, .right=right, .left_length=nl, .right_length=nr};
    MarimayAllocator a=mmay_system_allocator();
    MarimayBytecodeFunction body={0};
    MarimayBytecodeType params[]={MMAY_BYTECODE_TYPE_ARRAY, MMAY_BYTECODE_TYPE_I64};
    MarimayBytecodeType locals[]={MMAY_BYTECODE_TYPE_STRING, MMAY_BYTECODE_TYPE_STRING,
        MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_ARRAY};
    uint32_t added;
    EXPECT(mmay_bytecode_program_init(&f->program,a,1,mmay_bytecode_program_default_options()));
    EXPECT(mmay_bytecode_function_init(&body,a,MMAY_BYTECODE_TYPE_BOOL,1,params,2,
        mmay_bytecode_function_default_options()));
    for (size_t i=0;i<5;++i) EXPECT(mmay_bytecode_add_register(&body,locals[i],&added));
    EXPECT(mmay_bytecode_add_array_type(&body,MMAY_BYTECODE_TYPE_STRING,MMAY_BYTECODE_NO_LAYOUT,&f->layout));
    EXPECT(mmay_bytecode_set_register_layout(&body,0,f->layout));
    EXPECT(mmay_bytecode_set_register_layout(&body,6,f->layout));
    EXPECT(mmay_bytecode_add_block(&body,&added) && added==0);
    EXPECT(mmay_bytecode_add_block(&body,&added) && added==1);
    append(&body,0,MMAY_OPCODE_CONST_BOOL,5,0,0);
    append(&body,0,MMAY_OPCODE_BRANCH_BOOL,5,0,1);
    f->first_read=proven?3:2;
    read_string(f,&body);
    append(&body,1,MMAY_OPCODE_COPY,3,2,0);
    append(&body,1,MMAY_OPCODE_STRING_EQUAL,4,2,3);
    append(&body,1,MMAY_OPCODE_CONST_I64,1,1,0);
    read_string(f,&body);
    append(&body,1,MMAY_OPCODE_STRING_EQUAL,4,2,3);
    f->cold=body.instruction_count-2;
    MarimayBytecodeArrayConstructionId construction;
    EXPECT(mmay_bytecode_add_array_construction_with_layout(&body,MMAY_BYTECODE_TYPE_ARRAY,
        f->layout,NULL,0,&construction));
    append(&body,1,MMAY_OPCODE_ARRAY_NEW,0,construction,0);
    append(&body,1,MMAY_OPCODE_COPY,6,0,0);
    append(&body,1,MMAY_OPCODE_ARRAY_NEW,0,construction,0);
    append(&body,1,MMAY_OPCODE_STRING_EQUAL,4,2,3);
    append(&body,1,MMAY_OPCODE_RETURN,4,0,0);
    MarimaySymbolId symbol={{0x25}};
    EXPECT(mmay_bytecode_program_take_function(&f->program,symbol,&body,&added));
    EXPECT(mmay_bytecode_program_set_entry(&f->program,symbol));
    EXPECT(mmay_bytecode_program_verify(&f->program)==MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static const MarimayBytecodeFunction *body(const Fixture *f) {
    return &mmay_bytecode_program_function(&f->program,0)->body;
}

static bool equal(const Fixture *f) {
    return f->left_length==f->right_length && (f->left_length==0
        || memcmp(f->left,f->right,f->left_length)==0);
}

static void objects(const Fixture *f,MarimayBytecodeExecution *e,MarimayAllocator a,
    MarimayManagedReference refs[3]) {
    EXPECT(mmay_bytecode_execution_init(e,a,mmay_bytecode_execution_default_options()));
    for (size_t i=0;i<3;++i) refs[i]=mmay_managed_invalid_reference();
    MarimayManagedReference *locations[]={&refs[0],&refs[1],&refs[2]};
    MarimayManagedRootFrame roots={0};
    EXPECT(mmay_managed_root_push(&e->managed_heap,&roots,locations,3)==MMAY_MANAGED_OK);
    EXPECT(mmay_managed_string_new(&e->managed_heap,f->left,f->left_length,&refs[0])==MMAY_MANAGED_OK);
    EXPECT(mmay_managed_string_new(&e->managed_heap,f->right,f->right_length,&refs[1])==MMAY_MANAGED_OK);
    const MarimayManagedArrayDescriptor *d=mmay_bytecode_array_descriptor_for(e,body(f),
        MMAY_BYTECODE_TYPE_ARRAY,f->layout);
    MarimayManagedCell cells[]={{.reference=refs[0]},{.reference=refs[1]}};
    EXPECT(d && mmay_managed_array_new(&e->managed_heap,d,cells,2,&refs[2])==MMAY_MANAGED_OK);
    EXPECT(mmay_managed_root_pop(&e->managed_heap,&roots)==MMAY_MANAGED_OK);
    e->managed_heap.options.collect_before_allocation=true;
}

typedef struct Observation {
    MarimayBytecodeRunStatus status;
    MarimayBytecodeValue result;
    MarimayBytecodeTrap trap;
    MarimayBytecodeSpan panic;
    size_t remaining,attempts;
    bool bounds;
} Observation;

static Observation run(const Fixture *f,bool jit,int64_t index,size_t fuel,bool shared,size_t failure) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing,mmay_system_allocator(),SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failing);
    MarimayBytecodeExecution e={0};MarimayManagedReference refs[3];objects(f,&e,a,refs);
    MarimayBytecodeValue args[]={{.type=MMAY_BYTECODE_TYPE_ARRAY,.as.managed_reference=refs[2]},
        {.type=MMAY_BYTECODE_TYPE_I64,.as.i64=index}};
    MarimayBytecodePanic panic={0};EXPECT(mmay_bytecode_panic_init(&panic,a));
    Observation o={.result={.type=MMAY_BYTECODE_TYPE_I64,.as.i64=1234},.remaining=fuel};
    MarimayBytecodeRunOptions options=mmay_bytecode_run_default_options();
    options.execution=&e;options.trap=&o.trap;options.panic=&panic;
    options.max_steps=shared?SIZE_MAX:fuel;options.remaining_shared_steps=shared?&o.remaining:NULL;
    failing.attempts=0;failing.failure_index=failure;
    o.status=jit?mmay_bytecode_run_program(&f->program,args,2,a,options,&o.result)
        :mmay_bytecode_run(body(f),args,2,a,options,&o.result);
    o.attempts=failing.attempts;o.panic=panic.span;
    o.bounds=panic.code && strcmp(panic.code,"G4002")==0;
    if (failure<failing.attempts) EXPECT(o.status==MMAY_BYTECODE_RUN_ALLOCATION_FAILED
        && o.result.type==MMAY_BYTECODE_TYPE_I64 && o.result.as.i64==1234);
    EXPECT(e.managed_heap.roots==NULL);
    mmay_bytecode_panic_dispose(&panic);mmay_bytecode_execution_dispose(&e);
    EXPECT(failing.live_allocations==0 && failing.live_bytes==0);
    return o;
}

static void compare(const Fixture *f,int64_t index,size_t fuel,bool shared) {
    Observation a=run(f,false,index,fuel,shared,SIZE_MAX),b=run(f,true,index,fuel,shared,SIZE_MAX);
    EXPECT(a.status==b.status && a.result.type==b.result.type && a.remaining==b.remaining);
    if (a.status==MMAY_BYTECODE_RUN_OK) EXPECT(a.result.as.boolean==b.result.as.boolean
        && a.result.as.boolean==(index==1 || equal(f)));
    else EXPECT(a.result.as.i64==b.result.as.i64);
    EXPECT(a.trap.has_span==b.trap.has_span && a.bounds==b.bounds);
    if (a.trap.has_span) EXPECT(a.trap.span.start==b.trap.span.start && a.trap.span.end==b.trap.span.end);
    if (a.bounds) EXPECT(a.panic.start==b.panic.start && a.panic.end==b.panic.end);
}

static void native(const Fixture *f) {
    MarimayFailAllocator failing;mmay_fail_allocator_init(&failing,mmay_system_allocator(),SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failing);
    MarimayCodeMemoryFault pages={.failure_index=SIZE_MAX};
    MarimayBytecodeExecution e={0};MarimayManagedReference refs[3];objects(f,&e,a,refs);
    MarimayBytecodeExecutionImage image={0};MarimayBytecodeJit jit={0};
    EXPECT(mmay_bytecode_execution_image_init(&image,&f->program,a)==MMAY_BYTECODE_IMAGE_OK);
    EXPECT(mmay_bytecode_jit_init(&jit,&f->program,&image,a,&pages,true)==MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready && jit.functions[0].code && jit.functions[0].plan[f->first_read].supported);
    for (size_t i=0;i<image.functions[0].operation_count;++i)
        if (image.functions[0].operations[i].opcode==MMAY_OPCODE_STRING_EQUAL)
            EXPECT(jit.functions[0].plan[i].supported);
    MarimayBytecodeValue regs[7]={0};const MarimayBytecodeType *types=body(f)->register_types.data;
    for (size_t i=0;i<7;++i) {
        regs[i].type=types[i];
        if (mmay_bytecode_type_is_managed(types[i])) regs[i].as.managed_reference=mmay_managed_invalid_reference();
    }
    regs[0].as.managed_reference=refs[2];
    MarimayManagedReference *locations[]={&regs[0].as.managed_reference,&regs[2].as.managed_reference,
        &regs[3].as.managed_reference,&regs[6].as.managed_reference};
    MarimayManagedRootFrame roots={0};
    EXPECT(mmay_managed_root_push(&e.managed_heap,&roots,locations,4)==MMAY_MANAGED_OK);
    MarimayBytecodeRunOptions options=mmay_bytecode_run_default_options();options.max_steps=SIZE_MAX;options.execution=&e;
    for (size_t trial=0;trial<3;++trial) {
        size_t block=1,pc=0,steps=0;regs[1].as.i64=0;
        if (trial==1) regs[0].as.managed_reference.heap_provenance+=1;
        if (trial==2) {regs[0].as.managed_reference=refs[2];regs[0].as.managed_reference.generation+=1;}
        size_t attempts=failing.attempts;failing.failure_index=attempts;
        EXPECT(mmay_bytecode_jit_run(&jit,0,&image.functions[0],body(f),regs,&e,&block,&pc,&steps,options));
        EXPECT(failing.attempts==attempts && block==1 && pc==(trial?0:f->cold));
        EXPECT(regs[4].as.boolean==equal(f));
        EXPECT(mmay_managed_reference_equal(regs[2].as.managed_reference,refs[1]));
        failing.failure_index=SIZE_MAX;
    }
    regs[0].as.managed_reference=mmay_managed_invalid_reference();
    regs[6].as.managed_reference=mmay_managed_invalid_reference();
    EXPECT(mmay_managed_heap_collect(&e.managed_heap)==MMAY_MANAGED_OK);
    MarimayManagedObjectKind kind;
    EXPECT(mmay_managed_object_kind(&e.managed_heap,refs[2],&kind)==MMAY_MANAGED_INVALID_REFERENCE);
    size_t block=1,pc=f->cold+3,steps=0;
    EXPECT(mmay_bytecode_jit_run(&jit,0,&image.functions[0],body(f),regs,&e,&block,&pc,&steps,options));
    EXPECT(pc==f->cold+4 && regs[4].as.boolean==equal(f));
    EXPECT(mmay_managed_root_pop(&e.managed_heap,&roots)==MMAY_MANAGED_OK);
    mmay_bytecode_jit_dispose(&jit);mmay_bytecode_execution_image_dispose(&image);mmay_bytecode_execution_dispose(&e);
    EXPECT(failing.live_allocations==0 && failing.live_bytes==0 && pages.live_mappings==0 && pages.live_bytes==0);
}

static void views(const Fixture *f) {
    MarimayFailAllocator failing;mmay_fail_allocator_init(&failing,mmay_system_allocator(),SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failing);
    MarimayBytecodeExecution e={0},other={0};MarimayManagedReference refs[3];objects(f,&e,a,refs);
    EXPECT(mmay_bytecode_execution_init(&other,a,mmay_bytecode_execution_default_options()));
    MarimayBytecodeValue regs[7]={0};MarimayBytecodeJitBinding bindings[7]={0};
    regs[2].type=MMAY_BYTECODE_TYPE_STRING;regs[2].as.managed_reference=refs[0];
    MarimayManagedRootFrame roots={0};
    MarimayManagedReference *locations[]={&regs[2].as.managed_reference};
    EXPECT(mmay_managed_root_push(&e.managed_heap,&roots,locations,1)==MMAY_MANAGED_OK);
    MarimayBytecodeJitContext c={.registers=regs,.bindings=bindings,.function=body(f),.execution=&e,.epoch=1};
    MarimayManagedCell cell={.reference=refs[0]};
    size_t attempts=failing.attempts;failing.failure_index=attempts;
    EXPECT(mmay_bytecode_jit_string_load(&c,2,&cell));
    const unsigned char *bytes=bindings[2].view.string.bytes;
    EXPECT(bindings[2].view.string.length==f->left_length && failing.attempts==attempts);
    MarimayManagedReference bad[]={refs[0],refs[0],mmay_managed_invalid_reference(),refs[2]};
    bad[0].heap_provenance+=1;bad[1].generation+=1;
    for (size_t i=0;i<4;++i) {
        MarimayBytecodeJitBinding saved=bindings[2];cell.reference=bad[i];
        EXPECT(!mmay_bytecode_jit_string_load(&c,2,&cell));
        EXPECT(mmay_managed_reference_equal(regs[2].as.managed_reference,refs[0]));
        EXPECT(memcmp(&saved,&bindings[2],sizeof(saved))==0 && failing.attempts==attempts);
        regs[2].as.managed_reference=bad[i];++c.epoch;
        EXPECT(mmay_bytecode_jit_bind(&c,2)==NULL);
        regs[2].as.managed_reference=refs[0];
        EXPECT(mmay_bytecode_jit_bind(&c,2)==&bindings[2].view);
    }
    c.execution=&other;++c.epoch;EXPECT(mmay_bytecode_jit_bind(&c,2)==NULL);c.execution=&e;
    EXPECT(mmay_bytecode_jit_bind(&c,2)==&bindings[2].view);
    failing.failure_index=SIZE_MAX;
    uint64_t epoch=e.managed_heap.view_epoch;
    EXPECT(mmay_managed_heap_collect(&e.managed_heap)==MMAY_MANAGED_OK);
    EXPECT(e.managed_heap.view_epoch==epoch+1);
    ++c.epoch;EXPECT(mmay_bytecode_jit_bind(&c,2)==&bindings[2].view);
    EXPECT(bindings[2].view.string.bytes==bytes && bindings[2].view_epoch==e.managed_heap.view_epoch);
    epoch=e.managed_heap.view_epoch;regs[2].as.managed_reference.heap_provenance+=1;
    EXPECT(mmay_managed_heap_collect(&e.managed_heap)!=MMAY_MANAGED_OK);
    EXPECT(e.managed_heap.view_epoch==epoch+1);
    regs[2].as.managed_reference=refs[0];++c.epoch;
    EXPECT(mmay_bytecode_jit_bind(&c,2)==&bindings[2].view);
    e.managed_heap.view_epoch=UINT64_MAX;
    /* Saturation must rebind rather than trusting retained byte metadata. */
    bindings[2].view.string.length=SIZE_MAX;++c.epoch;
    EXPECT(mmay_bytecode_jit_bind(&c,2)==&bindings[2].view);
    EXPECT(bindings[2].view.string.length==f->left_length);
    regs[2].as.managed_reference=mmay_managed_invalid_reference();
    EXPECT(mmay_managed_heap_collect(&e.managed_heap)==MMAY_MANAGED_OK);
    regs[2].as.managed_reference=refs[0];++c.epoch;
    EXPECT(mmay_bytecode_jit_bind(&c,2)==NULL);
    EXPECT(mmay_managed_root_pop(&e.managed_heap,&roots)==MMAY_MANAGED_OK);
    mmay_bytecode_execution_dispose(&other);mmay_bytecode_execution_dispose(&e);
    EXPECT(failing.live_allocations==0 && failing.live_bytes==0);
}

int main(void) {
    if (!mmay_code_memory_supported()) return 77;
    const unsigned char *values[]={NULL,(const unsigned char *)"x",(const unsigned char *)"a\0b",
        (const unsigned char *)"a\0c",(const unsigned char *)"\xd0\xb6",(const unsigned char *)"\xd0\xb6"};
    size_t lengths[]={0,1,3,3,2,2};
    for (size_t proven=0;proven<2;++proven) for (size_t pair=0;pair<3;++pair) {
        Fixture f;init(&f,proven!=0,values[2*pair],lengths[2*pair],values[2*pair+1],lengths[2*pair+1]);
        native(&f);views(&f);
        int64_t indices[]={INT64_MIN,-1,0,1,2,INT64_MAX};
        for (size_t i=0;i<6;++i) for (size_t shared=0;shared<2;++shared) {
            compare(&f,indices[i],SIZE_MAX,shared!=0);
            for (size_t fuel=1;fuel<=22;++fuel) compare(&f,indices[i],fuel,shared!=0);
        }
        for (size_t meter=0;meter<2;++meter) for (size_t bounds=0;bounds<2;++bounds) {
            size_t n=run(&f,true,bounds?-1:0,meter?40:SIZE_MAX,false,SIZE_MAX).attempts;
            for (size_t i=0;i<=n;++i) run(&f,true,bounds?-1:0,meter?40:SIZE_MAX,false,i);
        }
        mmay_bytecode_program_dispose(&f.program);
    }
    puts("JIT strings: native checked/proven reads, empty/NUL/Unicode equality, overwrites, GC roots, original PC/fuel and all-index failures passed");
    return 0;
}
