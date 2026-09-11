#include "vm_jit_record_reads.h"

void mmay_bytecode_jit_record_reads_reset(MarimayBytecodeJitRecordReads *reads) {
    for (size_t reg=0;reg<reads->function->register_types.length;++reg)
        reads->values[reg].known=false;
}

void mmay_bytecode_jit_record_reads_init(MarimayBytecodeJitRecordReads *reads,
    const MarimayBytecodeFunction *function, MarimayBytecodeJitRecordRead *storage) {
    *reads=(MarimayBytecodeJitRecordReads){function,storage};
    mmay_bytecode_jit_record_reads_reset(reads);
}

void mmay_bytecode_jit_record_read_write(MarimayBytecodeJitRecordReads *reads,
    uint32_t reg) {
    for (size_t other=0;other<reads->function->register_types.length;++other)
        if (other==reg || (reads->values[other].known && reads->values[other].receiver==reg))
            reads->values[other].known=false;
}

uint32_t mmay_bytecode_jit_record_read_find(const MarimayBytecodeJitRecordReads *reads,
    uint32_t receiver, uint64_t field, uint32_t destination) {
    const MarimayBytecodeType *types=reads->function->register_types.data;
    const uint32_t *layouts=reads->function->register_layouts.data;
    for (size_t reg=0;reg<reads->function->register_types.length;++reg) {
        const MarimayBytecodeJitRecordRead *value=&reads->values[reg];
        if (value->known && value->receiver==receiver && value->field==field
            && types[reg]==types[destination] && layouts[reg]==layouts[destination])
            return (uint32_t)reg;
    }
    return UINT32_MAX;
}

void mmay_bytecode_jit_record_read_remember(MarimayBytecodeJitRecordReads *reads,
    uint32_t receiver, uint64_t field, uint32_t destination) {
    /* Reading next into the receiver itself changes the object named by the
     * expression key. It cannot describe a subsequent read of that new object. */
    if (receiver!=destination)
        reads->values[destination]=(MarimayBytecodeJitRecordRead){field,receiver,true};
}
