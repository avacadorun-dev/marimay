#ifndef MMAY_BYTECODE_VM_JIT_RECORD_READS_H
#define MMAY_BYTECODE_VM_JIT_RECORD_READS_H

#include "mmay/bytecode/function.h"

typedef struct MarimayBytecodeJitRecordRead {
    uint64_t field;
    uint32_t receiver;
    bool known;
} MarimayBytecodeJitRecordRead;

typedef struct MarimayBytecodeJitRecordReads {
    const MarimayBytecodeFunction *function;
    MarimayBytecodeJitRecordRead *values;
} MarimayBytecodeJitRecordReads;

/* Emission-only record-field value numbering. Receiver registers must remain
 * unchanged and record stores must be cold boundaries. No object is retained. */
void mmay_bytecode_jit_record_reads_init(MarimayBytecodeJitRecordReads *reads,
    const MarimayBytecodeFunction *function, MarimayBytecodeJitRecordRead *storage);
void mmay_bytecode_jit_record_reads_reset(MarimayBytecodeJitRecordReads *reads);
void mmay_bytecode_jit_record_read_write(MarimayBytecodeJitRecordReads *reads,
    uint32_t reg);
uint32_t mmay_bytecode_jit_record_read_find(const MarimayBytecodeJitRecordReads *reads,
    uint32_t receiver, uint64_t field, uint32_t destination);
void mmay_bytecode_jit_record_read_remember(MarimayBytecodeJitRecordReads *reads,
    uint32_t receiver, uint64_t field, uint32_t destination);

#endif
