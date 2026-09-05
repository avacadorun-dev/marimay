#include "test_formatter_support.h"

/* Owns boolean-expression layout boundaries, semantic round trips and
 * failure-atomic formatting at every deterministic allocation index. */
static void mmay_formatter_boolean_case(const char *source, const char *expected) {
    MarimayFormatterFixture original = {0}, formatted = {0};
    MarimayAstTree before = {0}, after = {0};
    size_t attempts = 0, iteration = 0;
    if (!mmay_formatter_fixture_init(&original, "boolean-input.my",
            (const unsigned char *)source, strlen(source))) {
        MMAY_EXPECT(false);
        return;
    }
    MMAY_EXPECT(original.parsed.diagnostics.length == 0);
    do {
        MarimayFailAllocator state;
        MarimayFormatResult result = {0};
        MarimayFormatStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(),
            iteration == 0 ? SIZE_MAX : iteration - 1);
        status = mmay_format_source(&original.lexed, &original.parsed,
            mmay_fail_allocator(&state), mmay_format_default_options(), &result);
        if (iteration == 0) {
            attempts = state.attempts;
            MMAY_EXPECT(status == MMAY_FORMAT_OK);
            MMAY_EXPECT(result.initialized);
            if (result.initialized) {
                MarimayFormatResult second = {0};
                if (result.output.length != strlen(expected)
                    || memcmp(result.output.data, expected, strlen(expected)) != 0) {
                    fprintf(stderr, "boolean layout mismatch:\n%.*s\n",
                        (int)result.output.length, result.output.data);
                    MMAY_EXPECT(false);
                }
                if (mmay_formatter_fixture_init(&formatted, "boolean-output.my",
                        result.output.data, result.output.length)) {
                    MMAY_EXPECT(formatted.parsed.diagnostics.length == 0);
                    MMAY_EXPECT(mmay_formatter_comments_equal(&original.lexed, &formatted.lexed));
                    MMAY_EXPECT(mmay_format_source(&formatted.lexed, &formatted.parsed,
                        mmay_system_allocator(), mmay_format_default_options(), &second) == MMAY_FORMAT_OK);
                    MMAY_EXPECT(second.initialized && !second.changed);
                    if (second.initialized) mmay_format_result_dispose(&second);
                    MMAY_EXPECT(mmay_ast_project(&original.parsed.tree, mmay_system_allocator(),
                        mmay_ast_default_options(), &before) == MMAY_AST_OK);
                    MMAY_EXPECT(mmay_ast_project(&formatted.parsed.tree, mmay_system_allocator(),
                        mmay_ast_default_options(), &after) == MMAY_AST_OK);
                    if (before.initialized && after.initialized) {
                        MMAY_EXPECT(mmay_formatter_ast_semantically_equal(
                            before.root, original.snapshot, after.root, formatted.snapshot));
                    }
                } else MMAY_EXPECT(false);
            }
        } else {
            MMAY_EXPECT(status == MMAY_FORMAT_ALLOCATION_FAILED);
            MMAY_EXPECT(!result.initialized);
        }
        if (result.initialized) mmay_format_result_dispose(&result);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        iteration += 1;
    } while (iteration <= attempts);
    MMAY_EXPECT(attempts > 0);
    {
        size_t part_limit;
        bool completed = false;
        for (part_limit = 1; part_limit <= 4096 && !completed; part_limit += 1) {
            MarimayFailAllocator state;
            MarimayFormatResult result = {0};
            MarimayFormatOptions options = mmay_format_default_options();
            MarimayFormatStatus status;
            options.max_document_parts = part_limit;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
            status = mmay_format_source(&original.lexed, &original.parsed,
                mmay_fail_allocator(&state), options, &result);
            completed = status == MMAY_FORMAT_OK;
            MMAY_EXPECT(completed || status == MMAY_FORMAT_LIMIT_REACHED);
            MMAY_EXPECT(result.initialized == completed);
            if (result.initialized) mmay_format_result_dispose(&result);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(completed);
    }
    if (after.initialized) mmay_ast_tree_dispose(&after);
    if (before.initialized) mmay_ast_tree_dispose(&before);
    mmay_formatter_fixture_dispose(&formatted);
    mmay_formatter_fixture_dispose(&original);
}

void mmay_test_formatter_boolean_groups(void) {
    mmay_formatter_boolean_case(
        "fn comments(a:bool,b:bool):bool{return match a{true=>a /* first\n"
        "keep */ && b,\n// second arm\nfalse=>a||b,};}",
        "fn comments(a: bool, b: bool): bool {\n"
        "  return match a {\n"
        "    true => a /* first\n"
        "keep */\n"
        "      && b,\n"
        "    // second arm\n"
        "    false => a || b,\n"
        "  };\n"
        "}\n"
    );
    mmay_formatter_boolean_case(
        "fn comments(a:bool,b:bool):bool{return a // first\n&& b /* second */\n|| a;}",
        "fn comments(a: bool, b: bool): bool {\n"
        "  return a // first\n"
        "    && b /* second */\n"
        "    || a;\n"
        "}\n"
    );
    mmay_formatter_boolean_case(
        "fn has(value:Result<[]string,string>):bool{return match value{"
        "Ok(values)=>values.length()==1&&values[0]==\"\",Err(_)=>false,};}\n",
        "fn has(value: Result<[]string, string>): bool {\n"
        "  return match value {\n"
        "    Ok(values) => values.length() == 1 && values[0] == \"\",\n"
        "    Err(_) => false,\n"
        "  };\n"
        "}\n"
    );
    mmay_formatter_boolean_case(
        "fn select(value:bool):bool{return match value{true=>"
        "firstConditionWithALongName()&&secondConditionWithALongName()&&thirdConditionWithALongName(),"
        "false=>false,};}\n",
        "fn select(value: bool): bool {\n"
        "  return match value {\n"
        "    true => firstConditionWithALongName()\n"
        "      && secondConditionWithALongName()\n"
        "      && thirdConditionWithALongName(),\n"
        "    false => false,\n"
        "  };\n"
        "}\n"
    );
    mmay_formatter_boolean_case(
        "fn nested(a:bool,b:bool):bool{return match a{"
        "true if a&&b=>{const pair=(a&&b,a||b);return match b{true=>a&&b,false=>a||b};},"
        "_=>choose(a&&b,Flags{left:a||b,right:(a&&b)||a}),};}\n",
        "fn nested(a: bool, b: bool): bool {\n"
        "  return match a {\n"
        "    true if a && b => {\n"
        "      const pair = (a && b, a || b);\n"
        "      return match b {\n"
        "        true => a && b,\n"
        "        false => a || b,\n"
        "      };\n"
        "    },\n"
        "    _ => choose(a && b, Flags { left: a || b, right: (a && b) || a }),\n"
        "  };\n"
        "}\n"
    );
    mmay_formatter_boolean_case(
        "fn comments(a:bool,b:bool):bool{return match a{true=>a /* keep */ && b,\n"
        "// second arm\nfalse=>a||b,};}\n",
        "fn comments(a: bool, b: bool): bool {\n"
        "  return match a {\n"
        "    true => a /* keep */ && b,\n"
        "    // second arm\n"
        "    false => a || b,\n"
        "  };\n"
        "}\n"
    );
    mmay_formatter_boolean_case(
        "fn conditions(a:bool,b:bool):bool{if a&&b{return a||b;}while a||b{break;}"
        "const label=$\"{a&&b}\";return a&&b||a;}\n",
        "fn conditions(a: bool, b: bool): bool {\n"
        "  if a && b {\n"
        "    return a || b;\n"
        "  }\n"
        "  while a || b {\n"
        "    break;\n"
        "  }\n"
        "  const label = $\"{a && b}\";\n"
        "  return a && b || a;\n"
        "}\n"
    );
}
