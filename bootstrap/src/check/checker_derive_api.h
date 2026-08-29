#ifndef CHECKER_DERIVE_API_H
#define CHECKER_DERIVE_API_H

#include "checker_internal.h"

bool mmay_checker_private_collect_eq_hash_derive(
    MarimayChecker *checker,
    const MarimayAstNode *declaration,
    bool *derives_eq_hash,
    bool *valid
);

#endif
