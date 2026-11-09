#ifndef MMAY_CHECK_FACTS_H
#define MMAY_CHECK_FACTS_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/owned_bytes.h"
#include "mmay/source/source_manager.h"
#include "mmay/types/type.h"

typedef enum MarimayCheckedSymbolKind {
    MMAY_CHECKED_SYMBOL_FUNCTION,
    MMAY_CHECKED_SYMBOL_CONSTANT,
    MMAY_CHECKED_SYMBOL_TYPE_ALIAS,
    MMAY_CHECKED_SYMBOL_RECORD,
    MMAY_CHECKED_SYMBOL_ENUM,
    MMAY_CHECKED_SYMBOL_INTERFACE,
    MMAY_CHECKED_SYMBOL_LOCAL,
    MMAY_CHECKED_SYMBOL_PARAMETER,
    MMAY_CHECKED_SYMBOL_FIELD
} MarimayCheckedSymbolKind;

/*
 * Successful-package definition fact. type belongs to the containing
 * result's type table. name and package_name are result-owned; snapshot is
 * borrowed from the caller-owned AST/source lifetime. Definition spans are
 * half-open bytes.
 */
typedef struct MarimayCheckedSymbol {
    MarimayOwnedBytes name;
    MarimayOwnedBytes package_name;
    MarimaySymbolId symbol;
    /*
     * Result-local type: function signature, constant value, alias target,
     * or record/enum nominal type according to kind. Never compare this ID
     * across independently produced MarimayCheckResult values.
     */
    MarimayTypeId type;
    const MarimaySourceSnapshot *snapshot;
    size_t start;
    size_t end;
    MarimayCheckedSymbolKind kind;
    bool is_public;
    /* Lexical locals/parameters and nominal record fields only. Other kinds
     * have zero metadata. Byte scopes are half-open; locals are available
     * from visible_from, after the initializer, and nearest depth wins. */
    MarimaySymbolId owner_symbol;
    size_t scope_start, scope_end, visible_from, scope_depth;
} MarimayCheckedSymbol;

/*
 * Successful-package reference occurrence. The symbol resolves through the
 * containing result; snapshot is borrowed from that result's source lifetime.
 * Occurrence spans are exact half-open identifier bytes.
 */
typedef struct MarimayCheckedOccurrence {
    MarimaySymbolId symbol;
    const MarimaySourceSnapshot *snapshot;
    size_t start;
    size_t end;
} MarimayCheckedOccurrence;

/*
 * Resolver-published exact dot-call candidate for one source snapshot and
 * nominal receiver. All byte strings are result-owned. An empty free_spelling
 * means the dot call is available but no free spelling is visible in scope.
 * No result-local MarimayTypeId crosses this query boundary.
 */
typedef struct MarimayCheckedCompletion {
    MarimayOwnedBytes name;
    MarimayOwnedBytes package_name;
    MarimayOwnedBytes free_spelling;
    MarimaySymbolId symbol;
    MarimaySymbolId receiver_symbol;
    const MarimaySourceSnapshot *snapshot;
} MarimayCheckedCompletion;

#endif
