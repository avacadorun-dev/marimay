#ifndef MMAY_CHECK_CHECKER_VISIBLE_API_H
#define MMAY_CHECK_CHECKER_VISIBLE_API_H

#include "mmay/check/result.h"

typedef struct MarimayChecker MarimayChecker;
/* Current AST and its resolver-owned import environment. */
bool mmay_check_publish_visible_symbols(MarimayChecker *checker);
void mmay_check_dispose_visible_symbols(MarimayCheckResult *result);

#endif
