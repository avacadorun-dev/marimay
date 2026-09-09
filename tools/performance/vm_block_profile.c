#include "profile.h"
#include "bytecode_internal.h"
#include <stdio.h>
#include <inttypes.h>

#define SLOTS 32768
typedef struct ProfileBlock {
    const MarimayBytecodeFunction *function;
    size_t block, length, core_ops, core_segments, extended_ops, extended_segments;
    uint64_t visits;
} ProfileBlock;
static ProfileBlock profiles[SLOTS];
static bool overflow;
uint64_t profile_op_counts[MMAY_OPCODE_COUNT];

static unsigned profile_family(const MarimayBytecodeFunction *f,
    const MarimayBytecodeInstruction *i) {
    switch(i->opcode) {
        case MMAY_OPCODE_CONST_I64: case MMAY_OPCODE_CONST_BOOL:
        case MMAY_OPCODE_CONST_INTEGER: case MMAY_OPCODE_CONST_F64:
        case MMAY_OPCODE_COPY: case MMAY_OPCODE_BOOL_NOT:
        case MMAY_OPCODE_BOOL_EQUAL: case MMAY_OPCODE_JUMP:
        case MMAY_OPCODE_BRANCH_BOOL: case MMAY_OPCODE_I64_ADD_CHECKED:
        case MMAY_OPCODE_I64_SUBTRACT_CHECKED: case MMAY_OPCODE_I64_MULTIPLY_CHECKED:
        case MMAY_OPCODE_I64_DIVIDE_CHECKED: case MMAY_OPCODE_I64_REMAINDER_CHECKED:
        case MMAY_OPCODE_I64_NEGATE_CHECKED: case MMAY_OPCODE_I64_ADD_PROVEN:
        case MMAY_OPCODE_I64_SUBTRACT_PROVEN: case MMAY_OPCODE_I64_MULTIPLY_PROVEN:
        case MMAY_OPCODE_I64_EQUAL: case MMAY_OPCODE_I64_LESS:
        case MMAY_OPCODE_I64_LESS_EQUAL: return 1;
        case MMAY_OPCODE_RECORD_GET: case MMAY_OPCODE_RECORD_SET:
        case MMAY_OPCODE_RECORD_IDENTITY_SAME: case MMAY_OPCODE_ENUM_IS_VARIANT:
        case MMAY_OPCODE_ENUM_REQUIRE_VARIANT: case MMAY_OPCODE_ENUM_GET_PAYLOAD:
            return 2;
        case MMAY_OPCODE_ARRAY_LENGTH: case MMAY_OPCODE_ARRAY_REQUIRE_INDEX:
        case MMAY_OPCODE_ARRAY_GENERATION: case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION:
            return 1;
        case MMAY_OPCODE_ARRAY_GET: case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS:
        case MMAY_OPCODE_ARRAY_SET: case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS: {
            bool store=i->opcode==MMAY_OPCODE_ARRAY_SET || i->opcode==MMAY_OPCODE_ARRAY_SET_IN_BOUNDS;
            size_t reg=(size_t)i->operands[store?0:1].unsigned_value;
            const MarimayBytecodeType *types=f->register_types.data;
            const uint32_t *layouts=f->register_layouts.data;
            uint32_t element_layout=0;
            MarimayBytecodeType element=mmay_bytecode_function_array_element_type(
                f,types[reg],layouts[reg],&element_layout);
            return mmay_bytecode_type_is_managed(element)?2:1;
        }
        default:return 0;
    }
}
void profile_block(const MarimayBytecodeFunction *f,size_t block) {
    size_t index=(((uintptr_t)f>>4) ^ (block*2654435761u)) % SLOTS;
    for(size_t probe=0;probe<SLOTS;probe++,index=(index+1)%SLOTS) {
        ProfileBlock *p=&profiles[index];
        if(p->function==f && p->block==block){p->visits++;return;}
        if(p->function!=NULL)continue;
        p->function=f;p->block=block;p->visits=1;
        const MarimayBytecodeBlock *b=mmay_vector_at_const(&f->blocks,block);
        const MarimayBytecodeInstruction *ops=b->instructions.data;
        p->length=b->instructions.length;
        bool core=false,extended=false;
        for(size_t n=0;n<p->length;n++) {
            unsigned family=profile_family(f,&ops[n]);
            bool next_core=family==1,next_extended=family!=0;
            p->core_ops+=next_core;p->extended_ops+=next_extended;
            p->core_segments+=next_core&&!core;
            p->extended_segments+=next_extended&&!extended;
            core=next_core;extended=next_extended;
        }
        return;
    }
    overflow=true;
}
void profile_dump(void) {
    fprintf(stderr,"PROFILE {\"overflow\":%s,\"opcodes\":{",overflow?"true":"false");
    bool comma=false;
    for(size_t op=1;op<MMAY_OPCODE_COUNT;op++) {
        if(profile_op_counts[op]==0)continue;
        fprintf(stderr,"%s\"%s\":%" PRIu64,comma?",":"",
            mmay_opcode_metadata((MarimayOpcode)op)->mnemonic,profile_op_counts[op]);
        comma=true;
    }
    fputs("},\"blocks\":[",stderr);comma=false;
    for(size_t n=0;n<SLOTS;n++) {
        const ProfileBlock *p=&profiles[n];
        if(p->visits==0)continue;
        fprintf(stderr,"%s[%" PRIu64 ",%zu,%zu,%zu,%zu,%zu]",comma?",":"",
            p->visits,p->length,p->core_ops,p->core_segments,p->extended_ops,p->extended_segments);
        comma=true;
    }
    fputs("]}\n",stderr);
}
