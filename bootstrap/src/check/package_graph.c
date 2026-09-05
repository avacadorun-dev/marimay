#include "mmay/check/graph.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "checker_api.h"
#include "checker_core_api.h"
#include "checker_json_api.h"
#include "package_imports_api.h"
#include "mmay/base/string_literal.h"
#include "mmay/generated/diagnostic_codes.h"

typedef struct MarimayCheckGraphEdge {
    MarimayOwnedBytes path;
    const MarimayAstTree *ast;
    const MarimayAstNode *node;
    size_t path_start;
    size_t path_end;
    size_t target;
} MarimayCheckGraphEdge;

typedef struct MarimayCheckGraphPackage {
    const MarimayCheckPackageInput *input;
    MarimayVector edges;
    MarimayCheckPackageSurface surface;
    unsigned char state;
} MarimayCheckGraphPackage;

typedef struct MarimayCheckGraph {
    MarimayAllocator allocator;
    MarimayCheckGraphOptions options;
    MarimayCheckResult *result;
    MarimayVector packages;
    MarimayVector order;
    MarimayVector surface_refs;
    MarimayVector visit_stack;
    size_t total_imports;
    size_t total_bindings;
    MarimayCheckStatus failure;
} MarimayCheckGraph;

static bool mmay_graph_equal(
    const void *left,
    size_t left_length,
    const void *right,
    size_t right_length
) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static int mmay_graph_compare_bytes(
    const void *left,
    size_t left_length,
    const void *right,
    size_t right_length
) {
    size_t shared = left_length < right_length ? left_length : right_length;
    int comparison = shared == 0 ? 0 : memcmp(left, right, shared);
    if (comparison != 0) {
        return comparison;
    }
    return left_length < right_length ? -1 : left_length > right_length;
}

static int mmay_graph_compare_packages(const void *left, const void *right) {
    const MarimayCheckGraphPackage *left_package = left;
    const MarimayCheckGraphPackage *right_package = right;
    return mmay_graph_compare_bytes(
        left_package->input->import_path,
        left_package->input->import_path_length,
        right_package->input->import_path,
        right_package->input->import_path_length
    );
}

static const MarimayAstTree *mmay_graph_first_ast(
    const MarimayCheckPackageInput *input
) {
    const MarimayAstTree *result = NULL;
    size_t index = 0;
    for (index = 0; input != NULL && index < input->ast_count; index += 1) {
        const MarimayAstTree *candidate = input->asts[index];
        if (!mmay_checker_private_check_ast_valid(candidate)) {
            return NULL;
        }
        if (result == NULL) {
            result = candidate;
        } else {
            const MarimayAstTree *pair[2] = {result, candidate};
            if (mmay_checker_private_check_compare_ast_paths(
                    &pair[1], &pair[0]
                ) < 0) {
                result = candidate;
            }
        }
    }
    return result;
}

static bool mmay_graph_add_diagnostic(
    MarimayCheckGraph *graph,
    const char *code,
    const char *message,
    const MarimayAstTree *ast,
    size_t start,
    size_t end
) {
    MarimayCheckDiagnostic diagnostic;
    if (graph->result->diagnostics.length
        >= graph->options.package_options.max_diagnostics) {
        graph->failure = MMAY_CHECK_LIMIT_REACHED;
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.snapshot = ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!mmay_vector_push(&graph->result->diagnostics, &diagnostic)) {
        graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static bool mmay_graph_add_owned_diagnostic(
    MarimayCheckGraph *graph,
    const char *code,
    const unsigned char *message,
    size_t message_length,
    const MarimayAstTree *ast,
    size_t start,
    size_t end
) {
    MarimayCheckDiagnostic diagnostic;
    if (graph->result->diagnostics.length
        >= graph->options.package_options.max_diagnostics) {
        graph->failure = MMAY_CHECK_LIMIT_REACHED;
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    if (!mmay_owned_bytes_copy(
            &diagnostic.owned_message,
            graph->allocator,
            message,
            message_length
        )) {
        graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
        return false;
    }
    diagnostic.code = code;
    diagnostic.message = (const char *)diagnostic.owned_message.data;
    diagnostic.snapshot = ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!mmay_vector_push(&graph->result->diagnostics, &diagnostic)) {
        mmay_owned_bytes_dispose(&diagnostic.owned_message);
        graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static bool mmay_graph_add_cycle_diagnostic(
    MarimayCheckGraph *graph,
    size_t target,
    const MarimayCheckGraphEdge *edge
) {
    static const unsigned char prefix[] =
        "local package dependency cycle: ";
    static const unsigned char arrow[] = " -> ";
    MarimayVector message;
    size_t start_index = 0;
    size_t index = 0;
    bool accepted = false;
    memset(&message, 0, sizeof(message));
    while (start_index < graph->visit_stack.length) {
        const size_t *item = mmay_vector_at_const(
            &graph->visit_stack, start_index
        );
        if (item != NULL && *item == target) {
            break;
        }
        start_index += 1;
    }
    if (start_index == graph->visit_stack.length
        || !mmay_vector_init(&message, graph->allocator,
            sizeof(unsigned char))
        || !mmay_vector_append(&message, prefix, sizeof(prefix) - 1)) {
        graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (index = start_index; index < graph->visit_stack.length; index += 1) {
        const size_t *package_index = mmay_vector_at_const(
            &graph->visit_stack, index
        );
        const MarimayCheckGraphPackage *package = package_index == NULL
            ? NULL : mmay_vector_at_const(&graph->packages, *package_index);
        if (package == NULL || (index != start_index
                && !mmay_vector_append(&message, arrow, sizeof(arrow) - 1))
            || !mmay_vector_append(
                &message,
                package->input->import_path,
                package->input->import_path_length
            )) {
            graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    {
        const MarimayCheckGraphPackage *package = mmay_vector_at_const(
            &graph->packages, target
        );
        if (package == NULL
            || !mmay_vector_append(&message, arrow, sizeof(arrow) - 1)
            || !mmay_vector_append(&message, package->input->import_path,
                package->input->import_path_length)) {
            graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    accepted = mmay_graph_add_owned_diagnostic(
        graph, MMAY_DIAGNOSTIC_PACKAGE_CYCLE,
        message.data, message.length,
        edge->ast, edge->path_start, edge->path_end
    );
cleanup:
    mmay_vector_dispose(&message);
    return accepted;
}

static size_t mmay_graph_find_package(
    const MarimayCheckGraph *graph,
    const void *path,
    size_t path_length
) {
    size_t low = 0;
    size_t high = graph->packages.length;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const MarimayCheckGraphPackage *package = mmay_vector_at_const(
            &graph->packages, middle
        );
        int comparison = mmay_graph_compare_bytes(
            package->input->import_path,
            package->input->import_path_length,
            path,
            path_length
        );
        if (comparison < 0) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low < graph->packages.length) {
        const MarimayCheckGraphPackage *package = mmay_vector_at_const(
            &graph->packages, low
        );
        if (mmay_graph_equal(package->input->import_path,
                package->input->import_path_length, path, path_length)) {
            return low;
        }
    }
    return SIZE_MAX;
}

static const MarimayAstNode *mmay_graph_child_node(
    const MarimayAstNode *node,
    MarimaySyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (ordinal == 0) {
                return element->as.node;
            }
            ordinal -= 1;
        }
    }
    return NULL;
}

static const MarimaySyntaxToken *mmay_graph_child_token(
    const MarimayAstNode *node,
    MarimayTokenKind kind,
    size_t ordinal
) {
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_TOKEN
            && element->as.token.kind == kind) {
            if (ordinal == 0) {
                return &element->as.token;
            }
            ordinal -= 1;
        }
    }
    return NULL;
}

static bool mmay_graph_collect_edge(
    MarimayCheckGraph *graph,
    MarimayCheckGraphPackage *package,
    const MarimayAstTree *ast,
    const MarimayAstNode *node
) {
    const MarimaySyntaxToken *path_token = mmay_graph_child_token(
        node, MMAY_TOKEN_STRING, 0
    );
    MarimayCheckGraphEdge edge;
    MarimayStringLiteralStatus decode_status;
    const unsigned char *source = mmay_source_bytes(ast->snapshot);
    memset(&edge, 0, sizeof(edge));
    edge.target = SIZE_MAX;
    if (path_token == NULL || path_token->synthetic || source == NULL
        || path_token->end > mmay_source_length(ast->snapshot)) {
        graph->failure = MMAY_CHECK_INVALID_ARGUMENT;
        return false;
    }
    decode_status = mmay_string_literal_decode(
        source + path_token->start,
        path_token->end - path_token->start,
        false,
        graph->allocator,
        &edge.path
    );
    if (decode_status != MMAY_STRING_LITERAL_OK) {
        graph->failure = decode_status == MMAY_STRING_LITERAL_ALLOCATION_FAILED
            ? MMAY_CHECK_ALLOCATION_FAILED : MMAY_CHECK_INVALID_ARGUMENT;
        return false;
    }
    edge.ast = ast;
    edge.node = node;
    edge.path_start = path_token->start;
    edge.path_end = path_token->end;
    if (edge.path.length != 0
        && memchr(edge.path.data, 0, edge.path.length) == NULL) {
        edge.target = mmay_graph_find_package(
            graph, edge.path.data, edge.path.length
        );
    }
    if (!mmay_vector_push(&package->edges, &edge)) {
        mmay_owned_bytes_dispose(&edge.path);
        graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
        return false;
    }
    graph->total_imports += 1;
    return true;
}

static int mmay_graph_compare_edges(const void *left, const void *right) {
    const MarimayCheckGraphEdge *left_edge = left;
    const MarimayCheckGraphEdge *right_edge = right;
    const MarimaySourceSnapshot *left_snapshot = left_edge->ast->snapshot;
    const MarimaySourceSnapshot *right_snapshot = right_edge->ast->snapshot;
    int comparison = mmay_graph_compare_bytes(
        mmay_source_path(left_snapshot),
        mmay_source_path_length(left_snapshot),
        mmay_source_path(right_snapshot),
        mmay_source_path_length(right_snapshot)
    );
    if (comparison != 0) {
        return comparison;
    }
    return left_edge->node->start < right_edge->node->start ? -1
        : left_edge->node->start > right_edge->node->start;
}

static bool mmay_graph_collect_edges(
    MarimayCheckGraph *graph,
    MarimayCheckGraphPackage *package
) {
    size_t ast_index = 0;
    for (ast_index = 0; ast_index < package->input->ast_count; ast_index += 1) {
        const MarimayAstTree *ast = package->input->asts[ast_index];
        size_t import_count = 0;
        size_t index = 0;
        for (index = 0; index < ast->root->child_count; index += 1) {
            const MarimayAstElement *element = mmay_ast_child(ast->root, index);
            if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
                && element->as.node != NULL
                && element->as.node->kind == MMAY_SYNTAX_IMPORT_DECL) {
                import_count += 1;
                if (import_count > graph->options.max_imports_per_file
                    || graph->total_imports
                        >= graph->options.max_total_imports) {
                    graph->failure = MMAY_CHECK_LIMIT_REACHED;
                    return false;
                }
                if (!mmay_graph_collect_edge(
                        graph, package, ast, element->as.node
                    )) {
                    return false;
                }
            }
        }
    }
    if (package->edges.length > 1) {
        qsort(package->edges.data, package->edges.length,
            package->edges.element_size, mmay_graph_compare_edges);
    }
    return true;
}

static bool mmay_graph_visit(
    MarimayCheckGraph *graph,
    size_t package_index,
    size_t depth
) {
    MarimayCheckGraphPackage *package = mmay_vector_at(
        &graph->packages, package_index
    );
    size_t target = 0;
    if (depth > graph->options.max_dependency_depth) {
        graph->failure = MMAY_CHECK_LIMIT_REACHED;
        return false;
    }
    if (package->state == 2) {
        return true;
    }
    if (package->state == 1) {
        return false;
    }
    package->state = 1;
    if (!mmay_vector_push(&graph->visit_stack, &package_index)) {
        graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
        return false;
    }
    for (target = 0; target < graph->packages.length; target += 1) {
        size_t edge_index = 0;
        for (edge_index = 0; edge_index < package->edges.length;
            edge_index += 1) {
            const MarimayCheckGraphEdge *edge = mmay_vector_at_const(
                &package->edges, edge_index
            );
            if (edge->target == SIZE_MAX) {
                return mmay_graph_add_diagnostic(
                    graph, MMAY_DIAGNOSTIC_UNKNOWN_PACKAGE,
                    "imported local package is not in the supplied graph",
                    edge->ast, edge->path_start, edge->path_end
                );
            }
            if (edge->target == target) {
                MarimayCheckGraphPackage *dependency = mmay_vector_at(
                    &graph->packages, target
                );
                if (dependency->state == 1) {
                    return mmay_graph_add_cycle_diagnostic(
                        graph, target, edge
                    );
                }
                if (dependency->state == 0
                    && !mmay_graph_visit(graph, target, depth + 1)) {
                    return false;
                }
                break;
            }
        }
    }
    package->state = 2;
    graph->visit_stack.length -= 1;
    if (!mmay_vector_push(&graph->order, &package_index)) {
        graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static bool mmay_graph_surface_item(
    const MarimayCheckGraph *graph,
    const MarimayCheckPackageSurface *surface,
    const unsigned char *name,
    size_t name_length,
    bool *is_public
) {
    size_t index = 0;
    bool found = false;
    *is_public = false;
    if (mmay_checker_json_export(surface->path, surface->path_length, name, name_length)) {
        *is_public = true;
        return true;
    }
    for (index = 0; index < surface->function_count; index += 1) {
        const MarimayCheckedFunction *item = mmay_checked_function(
            graph->result, surface->function_start + index
        );
        if (item != NULL && mmay_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            found = true;
            *is_public = *is_public || item->is_public;
        }
    }
    for (index = 0; index < surface->constant_count; index += 1) {
        const MarimayCheckedConstant *item = mmay_checked_constant(
            graph->result, surface->constant_start + index
        );
        if (item != NULL && mmay_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    for (index = 0; index < surface->type_alias_count; index += 1) {
        const MarimayCheckedTypeAlias *item = mmay_checked_type_alias(
            graph->result, surface->type_alias_start + index
        );
        if (item != NULL && mmay_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    for (index = 0; index < surface->record_count; index += 1) {
        const MarimayCheckedRecord *item = mmay_checked_record(
            graph->result, surface->record_start + index
        );
        if (item != NULL && mmay_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    for (index = 0; index < surface->enum_count; index += 1) {
        const MarimayCheckedEnum *item = mmay_checked_enum(
            graph->result, surface->enum_start + index
        );
        if (item != NULL && mmay_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    for (index = 0; index < surface->interface_count; index += 1) {
        const MarimayCheckedInterface *item = mmay_checked_interface(
            graph->result, surface->interface_start + index
        );
        if (item != NULL && mmay_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    if (found) {
        return true;
    }
    return false;
}

static bool mmay_graph_push_binding(
    MarimayCheckGraph *graph,
    MarimayVector *bindings,
    const MarimayCheckGraphEdge *edge,
    const MarimaySyntaxToken *original,
    const MarimaySyntaxToken *local,
    bool is_namespace
) {
    MarimayCheckImportBinding binding;
    const unsigned char *source = mmay_source_bytes(edge->ast->snapshot);
    const MarimayCheckPackageSurface *surface = NULL;
    bool is_public = true;
    size_t index = 0;
    memset(&binding, 0, sizeof(binding));
    if (source == NULL || local == NULL || local->synthetic
        || edge->target >= graph->packages.length) {
        graph->failure = MMAY_CHECK_INVALID_ARGUMENT;
        return false;
    }
    binding.ast = edge->ast;
    binding.surface_index = edge->target;
    binding.local = source + local->start;
    binding.local_length = local->end - local->start;
    binding.start = local->start;
    binding.end = local->end;
    binding.is_namespace = is_namespace;
    if (!is_namespace) {
        binding.original = source + original->start;
        binding.original_length = original->end - original->start;
        surface = ((const MarimayCheckPackageSurface *const *)
            graph->surface_refs.data)[edge->target];
        if (!mmay_graph_surface_item(graph, surface, binding.original,
                binding.original_length, &is_public)) {
            return mmay_graph_add_diagnostic(
                graph, MMAY_DIAGNOSTIC_UNKNOWN_IMPORT,
                "imported package has no item with this name",
                edge->ast, original->start, original->end
            );
        }
        if (!is_public) {
            return mmay_graph_add_diagnostic(
                graph, MMAY_DIAGNOSTIC_PRIVATE_IMPORT,
                "imported item is package-private",
                edge->ast, original->start, original->end
            );
        }
    }
    for (index = 0; index < bindings->length; index += 1) {
        const MarimayCheckImportBinding *existing = mmay_vector_at_const(
            bindings, index
        );
        if (existing->ast == binding.ast && mmay_graph_equal(
                existing->local, existing->local_length,
                binding.local, binding.local_length
            )) {
            return mmay_graph_add_diagnostic(
                graph, MMAY_DIAGNOSTIC_IMPORT_CONFLICT,
                "import binding conflicts with an earlier binding",
                edge->ast, binding.start, binding.end
            );
        }
    }
    if (graph->total_bindings >= graph->options.max_total_bindings) {
        graph->failure = MMAY_CHECK_LIMIT_REACHED;
        return false;
    }
    if (!mmay_vector_push(bindings, &binding)) {
        graph->failure = MMAY_CHECK_ALLOCATION_FAILED;
        return false;
    }
    graph->total_bindings += 1;
    return true;
}

static bool mmay_graph_build_bindings(
    MarimayCheckGraph *graph,
    const MarimayCheckGraphPackage *package,
    MarimayVector *bindings
) {
    size_t edge_index = 0;
    for (edge_index = 0; edge_index < package->edges.length; edge_index += 1) {
        const MarimayCheckGraphEdge *edge = mmay_vector_at_const(
            &package->edges, edge_index
        );
        const MarimayAstNode *clause = mmay_graph_child_node(
            edge->node, MMAY_SYNTAX_IMPORT_CLAUSE, 0
        );
        const MarimaySyntaxToken *namespace_name = mmay_graph_child_token(
            clause, MMAY_TOKEN_IDENTIFIER, 0
        );
        size_t item_index = 0;
        size_t item_count = 0;
        if (mmay_graph_child_token(clause, MMAY_TOKEN_STAR, 0) != NULL) {
            if (!mmay_graph_push_binding(graph, bindings, edge, NULL,
                    namespace_name, true)) {
                return false;
            }
            continue;
        }
        for (item_index = 0; item_index < clause->child_count;
            item_index += 1) {
            const MarimayAstElement *element = mmay_ast_child(clause, item_index);
            if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
                && element->as.node != NULL
                && element->as.node->kind == MMAY_SYNTAX_IMPORT_ITEM) {
                const MarimaySyntaxToken *original = mmay_graph_child_token(
                    element->as.node, MMAY_TOKEN_IDENTIFIER, 0
                );
                const MarimaySyntaxToken *alias = mmay_graph_child_token(
                    element->as.node, MMAY_TOKEN_IDENTIFIER, 1
                );
                item_count += 1;
                if (item_count > graph->options.max_items_per_import) {
                    graph->failure = MMAY_CHECK_LIMIT_REACHED;
                    return false;
                }
                if (!mmay_graph_push_binding(
                        graph, bindings, edge, original,
                        alias == NULL ? original : alias, false
                    )) {
                    return false;
                }
            }
        }
    }
    for (edge_index = 0; edge_index < bindings->length; edge_index += 1) {
        const MarimayCheckImportBinding *binding = mmay_vector_at_const(
            bindings, edge_index
        );
        size_t pass = 0;
        size_t ast_index = 0;
        for (pass = 0; pass < 2; pass += 1) {
            for (ast_index = 0; ast_index < package->input->ast_count;
                ast_index += 1) {
                const MarimayAstTree *ast = package->input->asts[ast_index];
                const unsigned char *source = mmay_source_bytes(ast->snapshot);
                size_t declaration_index = 0;
                if ((pass == 0) != (ast == binding->ast)) {
                    continue;
                }
                for (declaration_index = 0;
                    declaration_index < ast->root->child_count;
                    declaration_index += 1) {
                    const MarimayAstElement *element = mmay_ast_child(
                        ast->root, declaration_index
                    );
                    const MarimaySyntaxToken *name = NULL;
                    if (element == NULL
                        || element->kind != MMAY_AST_ELEMENT_NODE
                        || element->as.node == NULL
                        || element->as.node->kind == MMAY_SYNTAX_IMPORT_DECL) {
                        continue;
                    }
                    name = mmay_graph_child_token(
                        element->as.node, MMAY_TOKEN_IDENTIFIER, 0
                    );
                    if (name != NULL && !name->synthetic
                        && mmay_graph_equal(
                            binding->local, binding->local_length,
                            source + name->start, name->end - name->start
                        )) {
                        bool declaration_is_later = ast == binding->ast
                            && name->start > binding->start;
                        return mmay_graph_add_diagnostic(
                            graph, MMAY_DIAGNOSTIC_IMPORT_CONFLICT,
                            "import binding conflicts with a package declaration",
                            declaration_is_later ? ast : binding->ast,
                            declaration_is_later
                                ? name->start : binding->start,
                            declaration_is_later ? name->end : binding->end
                        );
                    }
                }
            }
        }
    }
    return true;
}

static void mmay_graph_capture_surface_start(
    const MarimayCheckResult *result,
    MarimayCheckPackageSurface *surface
) {
    surface->function_start = result->functions.length;
    surface->constant_start = result->constants.length;
    surface->type_alias_start = result->type_aliases.length;
    surface->record_start = result->records.length;
    surface->enum_start = result->enums.length;
    surface->interface_start = result->interfaces.length;
}

static void mmay_graph_capture_surface_count(
    const MarimayCheckResult *result,
    MarimayCheckPackageSurface *surface
) {
    surface->function_count = result->functions.length - surface->function_start;
    surface->constant_count = result->constants.length - surface->constant_start;
    surface->type_alias_count = result->type_aliases.length
        - surface->type_alias_start;
    surface->record_count = result->records.length - surface->record_start;
    surface->enum_count = result->enums.length - surface->enum_start;
    surface->interface_count = result->interfaces.length
        - surface->interface_start;
}

static void mmay_graph_dispose(MarimayCheckGraph *graph) {
    size_t package_index = 0;
    for (package_index = 0; package_index < graph->packages.length;
        package_index += 1) {
        MarimayCheckGraphPackage *package = mmay_vector_at(
            &graph->packages, package_index
        );
        size_t edge_index = 0;
        for (edge_index = 0; edge_index < package->edges.length;
            edge_index += 1) {
            MarimayCheckGraphEdge *edge = mmay_vector_at(
                &package->edges, edge_index
            );
            mmay_owned_bytes_dispose(&edge->path);
        }
        mmay_vector_dispose(&package->edges);
    }
    mmay_vector_dispose(&graph->order);
    mmay_vector_dispose(&graph->surface_refs);
    mmay_vector_dispose(&graph->visit_stack);
    mmay_vector_dispose(&graph->packages);
}

static MarimayCheckStatus mmay_graph_finish_diagnostics(
    MarimayCheckGraph *graph
) {
    if (!mmay_check_clear_semantics(graph->result)) {
        mmay_graph_dispose(graph);
        mmay_check_result_dispose(graph->result);
        return MMAY_CHECK_ALLOCATION_FAILED;
    }
    mmay_graph_dispose(graph);
    return MMAY_CHECK_OK;
}

MarimayCheckGraphOptions mmay_check_graph_default_options(void) {
    MarimayCheckGraphOptions options;
    options.max_packages = 4096;
    options.max_total_imports = 100000;
    options.max_total_bindings = 1000000;
    options.max_imports_per_file = 10000;
    options.max_items_per_import = 10000;
    options.max_dependency_depth = 1024;
    options.package_options = mmay_check_default_options();
    return options;
}

bool mmay_check_graph_options_valid(
    MarimayAllocator allocator,
    MarimayCheckGraphOptions options
) {
    return options.max_packages != 0 && options.max_total_imports != 0
        && options.max_total_bindings != 0
        && options.max_imports_per_file != 0
        && options.max_items_per_import != 0
        && options.max_dependency_depth != 0
        && mmay_check_options_valid(allocator, options.package_options);
}

static MarimayCheckStatus mmay_check_graph(
    const MarimayCheckPackageInput *inputs,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    bool all_packages,
    MarimayAllocator allocator,
    MarimayCheckGraphOptions options,
    MarimayCheckResult *result
) {
    MarimayCheckGraph graph;
    size_t package_index = 0;
    size_t root_index = SIZE_MAX;
    const MarimayAstTree *root_ast = NULL;
    memset(&graph, 0, sizeof(graph));
    if (result == NULL) {
        return MMAY_CHECK_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (inputs == NULL || package_count == 0
        || (!all_packages && (root_import_path == NULL
            || root_import_path_length == 0))
        || package_count > options.max_packages
        || !mmay_check_graph_options_valid(allocator, options)) {
        return package_count > options.max_packages
            ? MMAY_CHECK_LIMIT_REACHED : MMAY_CHECK_INVALID_ARGUMENT;
    }
    graph.allocator = allocator;
    graph.options = options;
    graph.result = result;
    graph.failure = MMAY_CHECK_OK;
    if (!mmay_vector_init(&graph.packages, allocator,
            sizeof(MarimayCheckGraphPackage))
        || !mmay_vector_init(&graph.order, allocator, sizeof(size_t))
        || !mmay_vector_init(&graph.surface_refs, allocator,
            sizeof(MarimayCheckPackageSurface *))
        || !mmay_vector_init(&graph.visit_stack, allocator, sizeof(size_t))) {
        mmay_graph_dispose(&graph);
        return MMAY_CHECK_ALLOCATION_FAILED;
    }
    for (package_index = 0; package_index < package_count; package_index += 1) {
        MarimayCheckGraphPackage package;
        const MarimayCheckPackageInput *input = &inputs[package_index];
        memset(&package, 0, sizeof(package));
        if (input->import_path_length
            > options.package_options.max_package_name_bytes) {
            mmay_graph_dispose(&graph);
            return MMAY_CHECK_LIMIT_REACHED;
        }
        if (input->import_path == NULL || input->import_path_length == 0
            || memchr(input->import_path, 0, input->import_path_length) != NULL
            || input->asts == NULL || input->ast_count == 0
            || input->ast_count > options.package_options.max_source_files
            || mmay_graph_first_ast(input) == NULL) {
            mmay_graph_dispose(&graph);
            return MMAY_CHECK_INVALID_ARGUMENT;
        }
        package.input = input;
        if (!mmay_vector_init(&package.edges, allocator,
                sizeof(MarimayCheckGraphEdge))
            || !mmay_vector_push(&graph.packages, &package)) {
            mmay_vector_dispose(&package.edges);
            mmay_graph_dispose(&graph);
            return MMAY_CHECK_ALLOCATION_FAILED;
        }
    }
    qsort(graph.packages.data, graph.packages.length,
        graph.packages.element_size, mmay_graph_compare_packages);
    for (package_index = 1; package_index < graph.packages.length;
        package_index += 1) {
        const MarimayCheckGraphPackage *previous = mmay_vector_at_const(
            &graph.packages, package_index - 1
        );
        const MarimayCheckGraphPackage *current = mmay_vector_at_const(
            &graph.packages, package_index
        );
        if (mmay_graph_compare_packages(previous, current) == 0) {
            mmay_graph_dispose(&graph);
            return MMAY_CHECK_INVALID_ARGUMENT;
        }
    }
    root_index = all_packages ? 0 : mmay_graph_find_package(
        &graph, root_import_path, root_import_path_length
    );
    if (root_index == SIZE_MAX) {
        mmay_graph_dispose(&graph);
        return MMAY_CHECK_INVALID_ARGUMENT;
    }
    root_ast = mmay_graph_first_ast(((const MarimayCheckGraphPackage *)
        graph.packages.data)[root_index].input);
    if (all_packages) {
        const MarimayCheckGraphPackage *root = mmay_vector_at_const(
            &graph.packages, root_index
        );
        root_import_path = root->input->import_path;
        root_import_path_length = root->input->import_path_length;
    }
    if (!mmay_check_result_init(result, allocator, options.package_options,
            root_ast->snapshot, root_import_path, root_import_path_length)) {
        mmay_graph_dispose(&graph);
        return MMAY_CHECK_ALLOCATION_FAILED;
    }
    for (package_index = 0; package_index < graph.packages.length;
        package_index += 1) {
        MarimayCheckGraphPackage *package = mmay_vector_at(
            &graph.packages, package_index
        );
        package->surface.path = (const unsigned char *)package->input->import_path;
        package->surface.path_length = package->input->import_path_length;
        {
            MarimayCheckPackageSurface *surface = &package->surface;
            if (!mmay_vector_push(&graph.surface_refs, &surface)) {
                graph.failure = MMAY_CHECK_ALLOCATION_FAILED;
                goto fail;
            }
        }
        if (!mmay_graph_collect_edges(&graph, package)) {
            goto fail;
        }
    }
    if (all_packages) {
        for (package_index = 0; package_index < graph.packages.length;
            package_index += 1) {
            MarimayCheckGraphPackage *package = mmay_vector_at(
                &graph.packages, package_index
            );
            if (package->state == 0
                && !mmay_graph_visit(&graph, package_index, 1)) {
                goto fail;
            }
        }
    } else if (!mmay_graph_visit(&graph, root_index, 1)) {
        goto fail;
    }
    if (result->diagnostics.length != 0) {
        return mmay_graph_finish_diagnostics(&graph);
    }
    for (package_index = 0; package_index < graph.order.length;
        package_index += 1) {
        const size_t *ordered_index = mmay_vector_at_const(
            &graph.order, package_index
        );
        MarimayCheckGraphPackage *package = mmay_vector_at(
            &graph.packages, *ordered_index
        );
        MarimayVector bindings;
        MarimayCheckImportEnvironment imports;
        MarimayCheckStatus status;
        memset(&bindings, 0, sizeof(bindings));
        memset(&imports, 0, sizeof(imports));
        if (!mmay_vector_init(&bindings, allocator,
                sizeof(MarimayCheckImportBinding))) {
            graph.failure = MMAY_CHECK_ALLOCATION_FAILED;
            goto fail;
        }
        if (!mmay_graph_build_bindings(&graph, package, &bindings)) {
            mmay_vector_dispose(&bindings);
            goto fail;
        }
        if (result->diagnostics.length != 0) {
            mmay_vector_dispose(&bindings);
            return mmay_graph_finish_diagnostics(&graph);
        }
        imports.surfaces = graph.surface_refs.data;
        imports.surface_count = graph.packages.length;
        imports.bindings = bindings.data;
        imports.binding_count = bindings.length;
        mmay_graph_capture_surface_start(result, &package->surface);
        status = mmay_checker_check_package_unit(
            package->input->asts,
            package->input->ast_count,
            package->input->import_path,
            package->input->import_path_length,
            &imports,
            false,
            allocator,
            options.package_options,
            result
        );
        mmay_vector_dispose(&bindings);
        if (status != MMAY_CHECK_OK) {
            graph.failure = status;
            goto fail;
        }
        if (result->functions.length
                > options.package_options.max_functions
            || result->constants.length
                > options.package_options.max_constants
            || result->type_aliases.length
                > options.package_options.max_type_aliases
            || result->records.length > options.package_options.max_records
            || result->enums.length > options.package_options.max_enums
            || result->interfaces.length
                > options.package_options.max_interfaces) {
            graph.failure = MMAY_CHECK_LIMIT_REACHED;
            goto fail;
        }
        mmay_graph_capture_surface_count(result, &package->surface);
        if (result->diagnostics.length != 0) {
            return mmay_graph_finish_diagnostics(&graph);
        }
    }
    mmay_graph_dispose(&graph);
    return MMAY_CHECK_OK;
fail:
    mmay_graph_dispose(&graph);
    if (graph.failure != MMAY_CHECK_OK) {
        if (graph.failure == MMAY_CHECK_INCOMPLETE_LANGUAGE
            && result->initialized && result->diagnostics.length != 0) {
            return graph.failure;
        }
        mmay_check_result_dispose(result);
        return graph.failure;
    }
    return MMAY_CHECK_OK;
}

MarimayCheckStatus mmay_check_package_graph(
    const MarimayCheckPackageInput *inputs,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    MarimayAllocator allocator,
    MarimayCheckGraphOptions options,
    MarimayCheckResult *result
) {
    return mmay_check_graph(
        inputs, package_count, root_import_path, root_import_path_length,
        false, allocator, options, result
    );
}

MarimayCheckStatus mmay_check_module_graph(
    const MarimayCheckPackageInput *inputs,
    size_t package_count,
    MarimayAllocator allocator,
    MarimayCheckGraphOptions options,
    MarimayCheckResult *result
) {
    return mmay_check_graph(
        inputs, package_count, NULL, 0, true, allocator, options, result
    );
}
