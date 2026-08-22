#ifndef MMAY_MIR_VERIFY_H
#define MMAY_MIR_VERIFY_H

#include "mmay/mir/function.h"

typedef enum MarimayMirVerifyStatus {
    MMAY_MIR_VERIFY_OK,
    MMAY_MIR_VERIFY_INVALID_ARGUMENT,
    MMAY_MIR_VERIFY_ALLOCATION_FAILED,
    MMAY_MIR_VERIFY_LIMIT_REACHED,
    MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW,
    MMAY_MIR_VERIFY_INVALID_REGISTER,
    MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER,
    MMAY_MIR_VERIFY_TYPE_MISMATCH
} MarimayMirVerifyStatus;

MarimayMirVerifyStatus mmay_mir_verify(const MarimayMirFunction *function);

#endif
