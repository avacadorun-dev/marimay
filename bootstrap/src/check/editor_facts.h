#ifndef MMAY_CHECK_EDITOR_FACTS_H
#define MMAY_CHECK_EDITOR_FACTS_H

#include "checker_internal.h"

typedef struct MarimayCheckLocal MarimayCheckLocal;

/* Facts are appended while the actual checker binds/resolves names. They
 * become queryable only after successful final package publication. */
bool mmay_check_editor_local(MarimayFunctionChecker *function,
    MarimayCheckLocal *local, bool parameter);
bool mmay_check_editor_local_occurrence(MarimayFunctionChecker *function,
    const MarimayCheckLocal *local, size_t start, size_t end);
bool mmay_check_editor_field_occurrence(MarimayChecker *checker,
    MarimaySymbolId record, MarimayCheckName name);
bool mmay_check_editor_publish_fields(MarimayChecker *checker);

#endif
