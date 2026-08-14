#ifndef CHECKER_PUBLICATION_API_H
#define CHECKER_PUBLICATION_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 void ruda_checker_private_checker_dispose_declarations(RudaChecker *checker);
 bool ruda_checker_private_checker_top_level_index_init(RudaChecker *checker);
 bool ruda_checker_private_checker_local_nominal_index_init(RudaChecker *checker);
 bool ruda_checker_private_check_symbol_fact_total(
    const RudaChecker *checker,
    size_t *result
);
 bool ruda_checker_private_check_push_symbol_seed(
    RudaChecker *checker,
    RudaVector *seeds,
    RudaCheckName name,
    RudaSymbolId symbol,
    RudaTypeId type,
    RudaCheckedSymbolKind kind,
    bool is_public
);
 bool ruda_checker_private_publish_symbol_facts(RudaChecker *checker);
#endif
