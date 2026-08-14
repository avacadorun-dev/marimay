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
 * result's type table. name is result-owned; snapshot is borrowed from the
 * caller-owned AST/source lifetime. Definition spans are half-open bytes.
 */
typedef struct RudaCheckedSymbol {
    RudaOwnedBytes name;
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

#endif
