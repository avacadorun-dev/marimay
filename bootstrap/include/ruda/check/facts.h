#ifndef RUDA_CHECK_FACTS_H
#define RUDA_CHECK_FACTS_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/owned_bytes.h"
#include "ruda/source/source_manager.h"
#include "ruda/types/type.h"

typedef enum RudaCheckedSymbolKind {
    RUDA_CHECKED_SYMBOL_FUNCTION,
    RUDA_CHECKED_SYMBOL_CONSTANT,
    RUDA_CHECKED_SYMBOL_TYPE_ALIAS,
    RUDA_CHECKED_SYMBOL_RECORD,
    RUDA_CHECKED_SYMBOL_ENUM,
    RUDA_CHECKED_SYMBOL_INTERFACE
} RudaCheckedSymbolKind;

/*
 * Successful-package definition fact. type belongs to the containing
 * result's type table. name and package_name are result-owned; snapshot is
 * borrowed from the caller-owned AST/source lifetime. Definition spans are
 * half-open bytes.
 */
typedef struct RudaCheckedSymbol {
    RudaOwnedBytes name;
    RudaOwnedBytes package_name;
    RudaSymbolId symbol;
    /*
     * Result-local type: function signature, constant value, alias target,
     * or record/enum nominal type according to kind. Never compare this ID
     * across independently produced RudaCheckResult values.
     */
    RudaTypeId type;
    const RudaSourceSnapshot *snapshot;
    size_t start;
    size_t end;
    RudaCheckedSymbolKind kind;
    bool is_public;
} RudaCheckedSymbol;

/*
 * Successful-package reference occurrence. The symbol resolves through the
 * containing result; snapshot is borrowed from that result's source lifetime.
 * Occurrence spans are exact half-open identifier bytes.
 */
typedef struct RudaCheckedOccurrence {
    RudaSymbolId symbol;
    const RudaSourceSnapshot *snapshot;
    size_t start;
    size_t end;
} RudaCheckedOccurrence;

/*
 * Resolver-published exact dot-call candidate for one source snapshot and
 * nominal receiver. All byte strings are result-owned. An empty free_spelling
 * means the dot call is available but no free spelling is visible in scope.
 * No result-local RudaTypeId crosses this query boundary.
 */
typedef struct RudaCheckedCompletion {
    RudaOwnedBytes name;
    RudaOwnedBytes package_name;
    RudaOwnedBytes free_spelling;
    RudaSymbolId symbol;
    RudaSymbolId receiver_symbol;
    const RudaSourceSnapshot *snapshot;
} RudaCheckedCompletion;

#endif
