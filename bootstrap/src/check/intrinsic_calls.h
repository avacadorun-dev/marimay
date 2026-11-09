#ifndef MMAY_CHECK_INTRINSIC_CALLS_H
#define MMAY_CHECK_INTRINSIC_CALLS_H

#include "mmay/check/intrinsics.h"

typedef struct MarimayChecker MarimayChecker;

/* Resolution has already succeeded. Snapshot is the checker's current AST. */
bool mmay_check_record_intrinsic_call(MarimayChecker *checker,
    const MarimayIntrinsicSignature *intrinsic, size_t start, size_t end);

#endif
