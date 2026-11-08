#include "test_parser_cases.h"

void mmay_test_parser(void) {
    mmay_test_parser_snapshot();
    mmay_test_parser_empty_source();
    mmay_test_parser_examples();
    mmay_test_parser_all_productions();
    mmay_test_parser_recovery();
    mmay_test_parser_delimiter_recovery();
    mmay_test_parser_declarations();
    mmay_test_parser_declaration_grammar();
    mmay_test_parser_keyword_boundaries();
    mmay_test_parser_declaration_recovery();
    mmay_test_parser_pratt_expressions();
    mmay_test_parser_expression_grammar();
    mmay_test_parser_expression_recovery();
    mmay_test_parser_pattern_grammar();
    mmay_test_parser_record_field_grammar();
    mmay_test_parser_exact_delimited_grammar();
    mmay_test_parser_statement_forms();
    mmay_test_parser_statement_recovery();
    mmay_test_parser_for_recovery();
    mmay_test_parser_option_condition();
    mmay_test_parser_type_grammar();
    mmay_test_parser_array_type_spelling();
    mmay_test_parser_nested_generic_boundaries();
    mmay_test_parser_incremental_reuse();
    mmay_test_parser_limits_and_allocation();
    mmay_test_parser_pattern_limits();
    mmay_test_parser_type_list_separators();
    mmay_test_parser_all_single_bytes();
}
