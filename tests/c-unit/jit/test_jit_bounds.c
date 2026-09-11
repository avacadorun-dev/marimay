#include "vm_jit_internal.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define EXPECT(c) do { if (!(c)) { fprintf(stderr,"JIT bounds: %s at %d\n",#c,__LINE__);exit(1); } } while(0)

static void append(MarimayBytecodeFunction *f,uint32_t block,MarimayOpcode code,
    uint32_t a,uint32_t b,uint32_t c) {
    MarimayBytecodeInstruction i={.opcode=code,.span={.start=f->instruction_count+10,.end=f->instruction_count+11}};
    i.operands[0].unsigned_value=a;i.operands[1].unsigned_value=b;i.operands[2].unsigned_value=c;
    EXPECT(mmay_bytecode_append_instruction(f,block,&i));
}
int main(void) {
    if(!mmay_code_memory_supported())return 77;
    MarimayAllocator system=mmay_system_allocator();
    MarimayBytecodeProgram program={0};MarimayBytecodeFunction f={0};uint32_t id;
    MarimayBytecodeType types[]={MMAY_BYTECODE_TYPE_ARRAY_F64,MMAY_BYTECODE_TYPE_I64};
    EXPECT(mmay_bytecode_program_init(&program,system,1,mmay_bytecode_program_default_options()));
    EXPECT(mmay_bytecode_function_init(&f,system,MMAY_BYTECODE_TYPE_I64,1,types,2,mmay_bytecode_function_default_options()));
    EXPECT(mmay_bytecode_add_register(&f,MMAY_BYTECODE_TYPE_ARRAY_F64,&id)&&id==2);
    EXPECT(mmay_bytecode_add_register(&f,MMAY_BYTECODE_TYPE_BOOL,&id)&&id==3);
    EXPECT(mmay_bytecode_add_block(&f,&id)&&id==0);
    EXPECT(mmay_bytecode_add_block(&f,&id)&&id==1);
    append(&f,0,MMAY_OPCODE_CONST_BOOL,3,0,0);
    append(&f,0,MMAY_OPCODE_BRANCH_BOOL,3,0,1);
    append(&f,1,MMAY_OPCODE_ARRAY_REQUIRE_INDEX,2,0,1);
    append(&f,1,MMAY_OPCODE_RETURN,1,0,0);
    MarimaySymbolId symbol={{0x9a}};
    EXPECT(mmay_bytecode_program_take_function(&program,symbol,&f,&id));
    EXPECT(mmay_bytecode_program_set_entry(&program,symbol));
    EXPECT(mmay_bytecode_program_verify(&program)==MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    const MarimayBytecodeFunction *body=&mmay_bytecode_program_function(&program,0)->body;
    MarimayFailAllocator failure;mmay_fail_allocator_init(&failure,system,SIZE_MAX);
    MarimayAllocator a=mmay_fail_allocator(&failure);MarimayBytecodeExecution execution={0};
    EXPECT(mmay_bytecode_execution_init(&execution,a,mmay_bytecode_execution_default_options()));
    MarimayManagedReference reference;MarimayManagedCell cell={.f64_bits=7};
    EXPECT(mmay_managed_array_new(&execution.managed_heap,&mmay_managed_array_f64_descriptor,&cell,1,&reference)==MMAY_MANAGED_OK);
    MarimayManagedArrayView view;
    EXPECT(mmay_managed_array_view_bind(&execution.managed_heap,reference,&mmay_managed_array_f64_descriptor,&view)==MMAY_MANAGED_OK);
    MarimayBytecodeExecutionImage image={0};MarimayCodeMemoryFault pages={.failure_index=SIZE_MAX};
    EXPECT(mmay_bytecode_execution_image_init(&image,&program,a)==MMAY_BYTECODE_IMAGE_OK);
    for(size_t metered=0;metered<2;++metered) {
        MarimayBytecodeJit jit={0};
        EXPECT(mmay_bytecode_jit_init(&jit,&program,&image,a,&pages,metered!=0)==MMAY_BYTECODE_RUN_OK);
        EXPECT(jit.ready&&jit.functions[0].plan[2].supported);
        const size_t lengths[]={0,1,INT64_MAX};
        const int64_t indices[]={INT64_MIN,INT64_MIN+1,-1,0,1,INT64_MAX-1,INT64_MAX};
        for(size_t l=0;l<sizeof(lengths)/sizeof(*lengths);++l) {
            /* Synthetic metadata isolates a guard-only native segment. No array
             * load, store, collection or ordinary execution occurs until restore. */
            *view.length=lengths[l];
            for(size_t n=0;n<sizeof(indices)/sizeof(*indices);++n) {
                MarimayBytecodeValue regs[]={
                    {.type=MMAY_BYTECODE_TYPE_ARRAY_F64,.as.managed_reference=reference},
                    {.type=MMAY_BYTECODE_TYPE_I64,.as.i64=indices[n]},
                    {.type=MMAY_BYTECODE_TYPE_ARRAY_F64,.as.managed_reference=mmay_managed_invalid_reference()},
                    {.type=MMAY_BYTECODE_TYPE_BOOL}};
                size_t block=1,pc=0,steps=0,budget=10,attempts=failure.attempts;
                failure.failure_index=attempts;
                MarimayBytecodeRunOptions options=mmay_bytecode_run_default_options();
                options.execution=&execution;options.max_steps=SIZE_MAX;
                if(metered)options.remaining_shared_steps=&budget;
                EXPECT(mmay_bytecode_jit_run(&jit,0,&image.functions[0],body,regs,&execution,&block,&pc,&steps,options));
                bool valid=indices[n]>=0&&(uint64_t)indices[n]<(uint64_t)lengths[l];
                EXPECT(block==1&&pc==(valid?1U:0U));
                EXPECT(budget==(metered&&valid?9U:10U));
                EXPECT(valid?mmay_managed_reference_equal(regs[2].as.managed_reference,reference)
                    :mmay_managed_reference_is_invalid(regs[2].as.managed_reference));
                EXPECT(failure.attempts==attempts);failure.failure_index=SIZE_MAX;
            }
            *view.length=1;
        }
        mmay_bytecode_jit_dispose(&jit);
    }
    mmay_bytecode_execution_image_dispose(&image);mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&program);
    EXPECT(failure.live_allocations==0&&failure.live_bytes==0);
    EXPECT(pages.live_mappings==0&&pages.live_bytes==0);
    puts("JIT bounds: one guard, empty/max lengths, signed extremes, fuel and transactional exits passed");
    return 0;
}
