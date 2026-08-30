#ifndef CHECKER_DERIVE_API_H
#define CHECKER_DERIVE_API_H

#include "checker_internal.h"

bool mmay_checker_private_collect_derives(
    MarimayChecker *checker,
    const MarimayAstNode *declaration,
    bool *derives_debug,
    bool *derives_eq,
    bool *derives_eq_hash,
    bool *valid
);

#endif
