#ifndef MMAY_CHECKER_INTRINSICS_API_H
#define MMAY_CHECKER_INTRINSICS_API_H

#include "mmay/check/intrinsics.h"

typedef struct MarimayChecker MarimayChecker;

const MarimayIntrinsicSignature *mmay_json_intrinsic_at(size_t index);
const MarimayIntrinsicSignature *mmay_json_intrinsic_find(
    const unsigned char *package, size_t package_length,
    const unsigned char *name, size_t name_length);
bool mmay_check_publish_intrinsics_for_ast(MarimayChecker *checker);
void mmay_check_dispose_intrinsic_values(MarimayCheckResult *result);
bool mmay_check_completion_budget_available(
    const MarimayCheckResult *result, size_t maximum);

#endif
