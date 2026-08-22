#ifndef CHECKER_PUBLICATION_API_H
#define CHECKER_PUBLICATION_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 void mmay_checker_private_checker_dispose_declarations(MarimayChecker *checker);
 bool mmay_checker_private_checker_top_level_index_init(MarimayChecker *checker);
 bool mmay_checker_private_checker_local_nominal_index_init(MarimayChecker *checker);
 bool mmay_checker_private_check_symbol_fact_total(
    const MarimayChecker *checker,
    size_t *result
);
 bool mmay_checker_private_check_push_symbol_seed(
    MarimayChecker *checker,
    MarimayVector *seeds,
    MarimayCheckName name,
    MarimaySymbolId symbol,
    MarimayTypeId type,
    MarimayCheckedSymbolKind kind,
    bool is_public
);
 bool mmay_checker_private_publish_symbol_facts(MarimayChecker *checker);
#endif
