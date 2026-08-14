#ifndef RUDA_TYPES_TABLE_H
#define RUDA_TYPES_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/map.h"
#include "ruda/base/vector.h"
#include "ruda/types/model.h"

typedef struct RudaTypeTableOptions {
    size_t max_types;
    size_t max_type_arguments;
} RudaTypeTableOptions;

typedef struct RudaTypeTable {
    RudaAllocator allocator;
    RudaVector descriptors;
    RudaMap canonical;
    RudaTypeTableOptions options;
    bool initialized;
} RudaTypeTable;

RudaTypeTableOptions ruda_type_table_default_options(void);

/* Stable across hosts for the same exact byte parts and discriminator. */
bool ruda_symbol_id_from_bytes(
    const void *package_name,
    size_t package_name_length,
    const void *source_path,
    size_t source_path_length,
    const void *declaration_name,
    size_t declaration_name_length,
    uint32_t discriminator,
    RudaSymbolId *result
);
bool ruda_symbol_id_from_parts(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    uint32_t discriminator,
    RudaSymbolId *result
);
/* Stable function identity for an exact nominal first-parameter owner. */
bool ruda_symbol_id_from_bytes_with_receiver(
    const void *package_name,
    size_t package_name_length,
    const void *source_path,
    size_t source_path_length,
    const void *declaration_name,
    size_t declaration_name_length,
    RudaSymbolId receiver,
    RudaSymbolId *result
);
bool ruda_symbol_id_from_parts_with_receiver(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    RudaSymbolId receiver,
    RudaSymbolId *result
);
bool ruda_symbol_id_equal(RudaSymbolId left, RudaSymbolId right);

bool ruda_type_table_init(
    RudaTypeTable *table,
    RudaAllocator allocator,
    RudaTypeTableOptions options
);
void ruda_type_table_dispose(RudaTypeTable *table);

RudaTypeKind ruda_type_kind(const RudaTypeTable *table, RudaTypeId type);
const RudaTypeDescriptor *ruda_type_descriptor(
    const RudaTypeTable *table,
    RudaTypeId type
);

bool ruda_type_intern_nominal(
    RudaTypeTable *table,
    RudaTypeKind kind,
    RudaSymbolId symbol,
    const RudaTypeId *arguments,
    size_t argument_count,
    RudaTypeId *result
);
bool ruda_type_intern_array(
    RudaTypeTable *table,
    RudaTypeId element,
    RudaTypeId *result
);
bool ruda_type_array_element(
    const RudaTypeTable *table,
    RudaTypeId array,
    RudaTypeId *element
);
bool ruda_type_intern_tuple(
    RudaTypeTable *table,
    const RudaTypeId *elements,
    size_t element_count,
    RudaTypeId *result
);
bool ruda_type_intern_function(
    RudaTypeTable *table,
    const RudaTypeId *parameters,
    size_t parameter_count,
    RudaTypeId return_type,
    RudaTypeId *result
);

#endif
