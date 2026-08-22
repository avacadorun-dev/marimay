#ifndef MMAY_TYPES_TABLE_H
#define MMAY_TYPES_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/map.h"
#include "mmay/base/vector.h"
#include "mmay/types/model.h"

typedef struct MarimayTypeTableOptions {
    size_t max_types;
    size_t max_type_arguments;
} MarimayTypeTableOptions;

typedef struct MarimayTypeTable {
    MarimayAllocator allocator;
    MarimayVector descriptors;
    MarimayMap canonical;
    MarimayTypeTableOptions options;
    bool initialized;
} MarimayTypeTable;

MarimayTypeTableOptions mmay_type_table_default_options(void);

/* Stable across hosts for the same exact byte parts and discriminator. */
bool mmay_symbol_id_from_bytes(
    const void *package_name,
    size_t package_name_length,
    const void *source_path,
    size_t source_path_length,
    const void *declaration_name,
    size_t declaration_name_length,
    uint32_t discriminator,
    MarimaySymbolId *result
);
bool mmay_symbol_id_from_parts(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    uint32_t discriminator,
    MarimaySymbolId *result
);
/* Stable function identity for an exact nominal first-parameter owner. */
bool mmay_symbol_id_from_bytes_with_receiver(
    const void *package_name,
    size_t package_name_length,
    const void *source_path,
    size_t source_path_length,
    const void *declaration_name,
    size_t declaration_name_length,
    MarimaySymbolId receiver,
    MarimaySymbolId *result
);
bool mmay_symbol_id_from_parts_with_receiver(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    MarimaySymbolId receiver,
    MarimaySymbolId *result
);
bool mmay_symbol_id_equal(MarimaySymbolId left, MarimaySymbolId right);

bool mmay_type_table_init(
    MarimayTypeTable *table,
    MarimayAllocator allocator,
    MarimayTypeTableOptions options
);
void mmay_type_table_dispose(MarimayTypeTable *table);

MarimayTypeKind mmay_type_kind(const MarimayTypeTable *table, MarimayTypeId type);
const MarimayTypeDescriptor *mmay_type_descriptor(
    const MarimayTypeTable *table,
    MarimayTypeId type
);

bool mmay_type_intern_nominal(
    MarimayTypeTable *table,
    MarimayTypeKind kind,
    MarimaySymbolId symbol,
    const MarimayTypeId *arguments,
    size_t argument_count,
    MarimayTypeId *result
);
bool mmay_type_intern_array(
    MarimayTypeTable *table,
    MarimayTypeId element,
    MarimayTypeId *result
);
bool mmay_type_array_element(
    const MarimayTypeTable *table,
    MarimayTypeId array,
    MarimayTypeId *element
);
bool mmay_type_intern_tuple(
    MarimayTypeTable *table,
    const MarimayTypeId *elements,
    size_t element_count,
    MarimayTypeId *result
);
bool mmay_type_intern_function(
    MarimayTypeTable *table,
    const MarimayTypeId *parameters,
    size_t parameter_count,
    MarimayTypeId return_type,
    MarimayTypeId *result
);

#endif
