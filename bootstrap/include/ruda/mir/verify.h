#ifndef RUDA_MIR_VERIFY_H
#define RUDA_MIR_VERIFY_H

#include "ruda/mir/function.h"

typedef enum RudaMirVerifyStatus {
    RUDA_MIR_VERIFY_OK,
    RUDA_MIR_VERIFY_INVALID_ARGUMENT,
    RUDA_MIR_VERIFY_ALLOCATION_FAILED,
    RUDA_MIR_VERIFY_LIMIT_REACHED,
    RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW,
    RUDA_MIR_VERIFY_INVALID_REGISTER,
    RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER,
    RUDA_MIR_VERIFY_TYPE_MISMATCH
} RudaMirVerifyStatus;

RudaMirVerifyStatus ruda_mir_verify(const RudaMirFunction *function);

#endif
