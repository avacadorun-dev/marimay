#ifndef MMAY_BYTECODE_VM_HOT_STRINGS_H
#define MMAY_BYTECODE_VM_HOT_STRINGS_H

#include "mmay/bytecode/vm.h"
#include "mmay/runtime/array_view.h"

typedef struct MarimayBytecodeHotStringView {
    MarimayManagedReference reference;
    const unsigned char *bytes;
    size_t length;
} MarimayBytecodeHotStringView;

/* One verified, rooted, nonallocating region only. No view survives a cold
 * exit, call, collection or execution/function change. Initialize count/next
 * to zero at every region entry; entries below count are fully initialized. */
typedef struct MarimayBytecodeHotStrings {
    MarimayBytecodeHotStringView entries[2];
    size_t count;
    size_t next;
} MarimayBytecodeHotStrings;

bool mmay_bytecode_hot_string_read(MarimayBytecodeHotStrings *cache,
    MarimayBytecodeExecution *execution, const MarimayManagedArrayView *array,
    int64_t index, MarimayManagedArrayAccessMode mode,
    MarimayBytecodeValue *target);
bool mmay_bytecode_hot_string_equal(MarimayBytecodeHotStrings *cache,
    MarimayBytecodeExecution *execution, MarimayBytecodeValue left,
    MarimayBytecodeValue right, MarimayBytecodeValue *target);

#endif
