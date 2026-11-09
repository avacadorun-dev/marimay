#ifndef MMAY_CHECK_VISIBLE_SYMBOLS_H
#define MMAY_CHECK_VISIBLE_SYMBOLS_H

#include "mmay/base/owned_bytes.h"
#include "mmay/source/source_manager.h"
#include "mmay/types/type.h"

struct MarimayCheckResult;

/* Actual file-import spelling or package-owned top-level name. Owned by result;
 * snapshot is borrowed. Local lexical shadowing is applied by the cursor query. */
typedef struct MarimayCheckedVisibleSymbol {
    MarimayOwnedBytes spelling;
    MarimaySymbolId symbol;
    const MarimaySourceSnapshot *snapshot;
} MarimayCheckedVisibleSymbol;

size_t mmay_checked_visible_symbol_count(const struct MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot, const void *prefix, size_t length);
const MarimayCheckedVisibleSymbol *mmay_checked_visible_symbol(
    const struct MarimayCheckResult *result, const MarimaySourceSnapshot *snapshot,
    const void *prefix, size_t length, size_t index);

#endif
