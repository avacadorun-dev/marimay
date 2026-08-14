#include "test_formatter_cases.h"

void ruda_test_formatter(void) {
    ruda_test_formatter_round_trip();
    ruda_test_formatter_fixture_file();
    ruda_test_formatter_long_list();
    ruda_test_formatter_example_corpus();
    ruda_test_formatter_trivia_property();
    ruda_test_formatter_rejection_limits_and_allocation();
    ruda_test_formatter_rejects_invalid_types();
}
