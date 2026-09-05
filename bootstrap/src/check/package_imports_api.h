#ifndef MMAY_CHECK_PACKAGE_IMPORTS_API_H
#define MMAY_CHECK_PACKAGE_IMPORTS_API_H

#include "checker_declaration_types.h"

typedef struct MarimayChecker MarimayChecker;

typedef struct MarimayCheckPackageSurface {
    const unsigned char *path;
    size_t path_length;
    size_t function_start;
    size_t function_count;
    size_t constant_start;
    size_t constant_count;
    size_t type_alias_start;
    size_t type_alias_count;
    size_t record_start;
    size_t record_count;
    size_t enum_start;
    size_t enum_count;
    size_t interface_start;
    size_t interface_count;
} MarimayCheckPackageSurface;

typedef struct MarimayCheckImportBinding {
    const MarimayAstTree *ast;
    size_t surface_index;
    const unsigned char *original;
    size_t original_length;
    const unsigned char *local;
    size_t local_length;
    size_t start;
    size_t end;
    bool is_namespace;
} MarimayCheckImportBinding;

typedef struct MarimayCheckImportEnvironment {
    const MarimayCheckPackageSurface *const *surfaces;
    size_t surface_count;
    const MarimayCheckImportBinding *bindings;
    size_t binding_count;
} MarimayCheckImportEnvironment;

typedef struct MarimayCheckCallable {
    const MarimayTypeId *parameter_types;
    size_t parameter_count;
    MarimaySymbolId symbol;
    MarimaySymbolId receiver_symbol;
    MarimayTypeId receiver_type;
    MarimayTypeId result_type;
    bool dot_eligible;
    bool is_external;
} MarimayCheckCallable;

bool mmay_check_import_binding_exists(
    const MarimayChecker *checker,
    const unsigned char *name,
    size_t name_length
);
bool mmay_check_import_namespace_exists(
    const MarimayChecker *checker,
    const unsigned char *name,
    size_t name_length
);
bool mmay_check_import_namespace_owns_enum(
    const MarimayChecker *checker,
    MarimayCheckName namespace_name,
    MarimayTypeId type
);
bool mmay_check_import_type(
    MarimayChecker *checker,
    const MarimayAstNode *type_node,
    MarimayTypeId *type,
    bool *resolved
);
bool mmay_check_imported_nominal(
    const MarimayChecker *checker,
    MarimayTypeId type,
    MarimaySymbolId *symbol
);
MarimayCheckCallResolution mmay_check_resolve_imported_call(
    const MarimayChecker *checker,
    MarimayCheckName namespace_name,
    bool has_namespace,
    MarimayCheckName name,
    bool receiver_known,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
);
MarimayCheckCallResolution mmay_check_resolve_imported_named_call(
    const MarimayChecker *checker,
    MarimayCheckName name,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
);
const MarimayCheckedRecord *mmay_check_imported_record_by_type(
    const MarimayChecker *checker,
    MarimayTypeId type
);
const MarimayCheckedRecord *mmay_check_imported_record_by_name(
    const MarimayChecker *checker,
    MarimayCheckName namespace_name,
    bool has_namespace,
    MarimayCheckName name,
    MarimayTypeId *type,
    bool *exists_private
);
const MarimayCheckedEnum *mmay_check_imported_enum_by_type(
    const MarimayChecker *checker,
    MarimayTypeId type
);
/* Optional alias_target_type retains the full concrete alias instantiation;
 * it is MMAY_TYPE_INVALID when a declaration, rather than an alias, is found. */
const MarimayCheckedEnum *mmay_check_imported_enum_by_name(
    const MarimayChecker *checker,
    MarimayCheckName namespace_name,
    bool has_namespace,
    MarimayCheckName enum_name,
    bool *resolved,
    bool *exists_private,
    MarimayTypeId *alias_target_type
);

#endif
