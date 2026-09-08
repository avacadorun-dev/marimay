# Generated Marimay bytecode opcode registry

<!-- Generated from bootstrap/tables/opcodes.tsv. Do not edit by hand. -->

This registry assigns explicit stable wire identifiers to the currently
implemented internal typed-bytecode foundation. It does not by itself define a
complete `.myb` bundle, VM execution support, or a public compatibility
promise. Operands use canonical bounded variable-length encodings in the
Stage 0 codec.

| Wire ID | Mnemonic | Operands | Flags | Meaning |
|---:|---|---|---|---|
| 0 | `invalid` | — | `invalid` | Reserved invalid opcode; it is never accepted in a function body. |
| 1 | `const_i64` | `destination:register`, `value:i64` | — | Load a signed 64-bit integer constant into a typed register. |
| 2 | `const_bool` | `destination:register`, `value:bool` | — | Load a boolean constant into a typed register. |
| 3 | `copy` | `destination:register`, `source:register` | — | Copy a value between registers of the same exact type. |
| 4 | `i64_add_checked` | `destination:register`, `left:register`, `right:register` | `may_panic` | Add signed 64-bit operands with checked overflow. |
| 5 | `i64_subtract_checked` | `destination:register`, `left:register`, `right:register` | `may_panic` | Subtract signed 64-bit operands with checked overflow. |
| 6 | `i64_multiply_checked` | `destination:register`, `left:register`, `right:register` | `may_panic` | Multiply signed 64-bit operands with checked overflow. |
| 7 | `i64_divide_checked` | `destination:register`, `left:register`, `right:register` | `may_panic` | Divide signed 64-bit operands after D042 zero and overflow checks. |
| 8 | `i64_remainder_checked` | `destination:register`, `left:register`, `right:register` | `may_panic` | Compute signed 64-bit remainder after D042 zero and overflow checks. |
| 9 | `i64_negate_checked` | `destination:register`, `source:register` | `may_panic` | Negate a signed 64-bit operand with checked overflow. |
| 10 | `bool_not` | `destination:register`, `source:register` | — | Compute boolean negation. |
| 11 | `i64_equal` | `destination:register`, `left:register`, `right:register` | — | Compare identical integer types or two runes for equality, producing bool. |
| 12 | `bool_equal` | `destination:register`, `left:register`, `right:register` | — | Compare boolean operands for equality. |
| 13 | `i64_less` | `destination:register`, `left:register`, `right:register` | — | Compare signed 64-bit operands using less-than. |
| 14 | `i64_less_equal` | `destination:register`, `left:register`, `right:register` | — | Compare signed 64-bit operands using less-than-or-equal. |
| 15 | `jump` | `target:block` | `terminator`, `branch` | Transfer control to one block. |
| 16 | `branch_bool` | `condition:register`, `then:block`, `otherwise:block` | `terminator`, `branch` | Transfer control according to an exact boolean register. |
| 17 | `return` | `value:register` | `terminator` | Return one typed register value. |
| 18 | `return_void` | — | `terminator` | Return normally from a void function. |
| 19 | `i64_bitwise_not` | `destination:register`, `source:register` | — | Compute portable two's-complement bitwise complement of a signed 64-bit operand. |
| 20 | `i64_bitwise_and` | `destination:register`, `left:register`, `right:register` | — | Compute portable two's-complement bitwise AND of signed 64-bit operands. |
| 21 | `i64_bitwise_xor` | `destination:register`, `left:register`, `right:register` | — | Compute portable two's-complement bitwise XOR of signed 64-bit operands. |
| 22 | `i64_bitwise_or` | `destination:register`, `left:register`, `right:register` | — | Compute portable two's-complement bitwise OR of signed 64-bit operands. |
| 23 | `i64_shift_left_checked` | `destination:register`, `left:register`, `count:register` | `may_panic` | Shift signed 64-bit left under D035 count and representability checks. |
| 24 | `i64_shift_right` | `destination:register`, `left:register`, `count:register` | `may_panic` | Shift signed 64-bit right with D035 count checks and explicit sign extension. |
| 25 | `call_direct` | `destination:register`, `call:call` | `may_panic` | Invoke a verified direct call descriptor and publish its exact typed result. |
| 26 | `call_direct_void` | `call:call` | `may_panic` | Invoke a verified direct call descriptor that returns void. |
| 27 | `const_string` | `destination:register`, `constant:string` | — | Materialize an immutable UTF-8 string constant in an execution context. |
| 28 | `string_equal` | `destination:register`, `left:register`, `right:register` | — | Compare complete UTF-8 string bytes without normalization or NUL termination. |
| 29 | `panic_string` | `message:register` | `terminator`, `may_panic` | Terminate with explicit-source panic G4000 and the exact string message. |
| 30 | `call_direct_never` | `call:call` | `terminator`, `may_panic` | Invoke a verified declared-never direct call with no normal continuation. |
| 31 | `const_f64` | `destination:register`, `value:f64_bits` | — | Load an IEEE 754 binary64 constant by its exact raw 64-bit bit pattern. |
| 32 | `f64_add` | `destination:register`, `left:register`, `right:register` | — | Add IEEE 754 binary64 operands under strict floating-point semantics. |
| 33 | `f64_subtract` | `destination:register`, `left:register`, `right:register` | — | Subtract IEEE 754 binary64 operands under strict floating-point semantics. |
| 34 | `f64_multiply` | `destination:register`, `left:register`, `right:register` | — | Multiply IEEE 754 binary64 operands under strict floating-point semantics. |
| 35 | `f64_divide` | `destination:register`, `left:register`, `right:register` | — | Divide IEEE 754 binary64 operands under strict IEEE division-by-zero semantics. |
| 36 | `f64_negate` | `destination:register`, `source:register` | — | Negate an IEEE 754 binary64 operand while preserving its exact result bits. |
| 37 | `f64_equal` | `destination:register`, `left:register`, `right:register` | — | Compare IEEE 754 binary64 operands for equality. |
| 38 | `f64_less` | `destination:register`, `left:register`, `right:register` | — | Compare IEEE 754 binary64 operands using less-than. |
| 39 | `f64_less_equal` | `destination:register`, `left:register`, `right:register` | — | Compare IEEE 754 binary64 operands using less-than-or-equal. |
| 40 | `array_new` | `destination:register`, `construction:array_construction` | `may_panic` | Create an exact managed array from a verified construction descriptor. |
| 41 | `array_length` | `destination:register`, `array:register` | — | Read an exact managed array length as i64. |
| 42 | `array_require_index` | `destination:register`, `array:register`, `index:register` | `may_panic` | Check D045 bounds and copy the array only after success. |
| 43 | `array_get` | `destination:register`, `array:register`, `index:register` | `may_panic` | Read one exact managed array element after a D045 bounds check. |
| 44 | `array_set` | `array:register`, `index:register`, `value:register` | `may_panic` | Store through an ARRAY_REQUIRE_INDEX carrier verified for the same D045 index. |
| 45 | `record_new` | `destination:register`, `construction:record_construction` | `may_panic` | Create a managed nominal record from a verified construction descriptor. |
| 46 | `record_get` | `destination:register`, `record:register`, `field:i64` | — | Read one record field by verified declaration-order index. |
| 47 | `record_set` | `destination:register`, `record:register`, `field:i64` | `may_panic` | Store the destination register into a mutable record field. |
| 48 | `record_identity_same` | `destination:register`, `left:register`, `right:register` | — | Compare two same-nominal record handles for managed identity. |
| 49 | `enum_new` | `destination:register`, `construction:enum_construction` | `may_panic` | Create a managed algebraic enum from a verified construction descriptor. |
| 50 | `enum_is_variant` | `destination:register`, `enum:register`, `variant:i64` | — | Test whether an enum value has the named variant tag. |
| 51 | `enum_require_variant` | `destination:register`, `enum:register`, `variant:i64` | — | Copy an enum handle after verifying its variant tag. |
| 52 | `enum_get_payload` | `destination:register`, `enum:register`, `selector:i64` | — | Read one enum payload field after a matching ENUM_REQUIRE_VARIANT. |
| 53 | `interface_new` | `destination:register`, `construction:interface_construction` | `may_panic` | Box a conforming payload with a compiler-generated method table. |
| 54 | `call_interface` | `destination:register`, `call:call` | `may_panic` | Dispatch an interface method through the boxed method table to a D003 callee. |
| 55 | `array_push` | `array:register`, `value:register` | `may_panic` | Append one exact element to a managed array with failure-atomic growth. |
| 56 | `const_integer` | `destination:register`, `value:u64` | — | Load an exact-width integer bit pattern into a typed integer register. |
| 57 | `integer_checked_add` | `destination:register`, `left:register`, `right:register` | `may_panic` | Return Some(sum) or None for checked integer failure. |
| 58 | `integer_checked_subtract` | `destination:register`, `left:register`, `right:register` | `may_panic` | Return Some(difference) or None for checked integer failure. |
| 59 | `integer_checked_multiply` | `destination:register`, `left:register`, `right:register` | `may_panic` | Return Some(product) or None for checked integer failure. |
| 60 | `integer_checked_divide` | `destination:register`, `left:register`, `right:register` | `may_panic` | Return Some(quotient) or None for checked integer failure. |
| 61 | `integer_checked_remainder` | `destination:register`, `left:register`, `right:register` | `may_panic` | Return Some(remainder) or None for checked integer failure. |
| 62 | `integer_checked_negate` | `destination:register`, `source:register` | `may_panic` | Return Some(negated value) or None for checked integer failure. |
| 63 | `integer_checked_shift_left` | `destination:register`, `left:register`, `right:register` | `may_panic` | Return Some(left shift) or None for checked integer failure. |
| 64 | `integer_checked_shift_right` | `destination:register`, `left:register`, `right:register` | `may_panic` | Return Some(right shift) or None for an invalid count. |
| 65 | `integer_wrapping_add` | `destination:register`, `left:register`, `right:register` | — | Add exact-width integers modulo their bit width. |
| 66 | `integer_wrapping_subtract` | `destination:register`, `left:register`, `right:register` | — | Subtract exact-width integers modulo their bit width. |
| 67 | `integer_wrapping_multiply` | `destination:register`, `left:register`, `right:register` | — | Multiply exact-width integers modulo their bit width. |
| 68 | `integer_wrapping_divide` | `destination:register`, `left:register`, `right:register` | `may_panic` | Divide with D047 wrapping overflow and zero-divisor policy. |
| 69 | `integer_wrapping_remainder` | `destination:register`, `left:register`, `right:register` | `may_panic` | Compute remainder with D047 wrapping overflow and zero-divisor policy. |
| 70 | `integer_wrapping_negate` | `destination:register`, `source:register` | — | Negate exact-width integers modulo their bit width. |
| 71 | `integer_wrapping_shift_left` | `destination:register`, `left:register`, `right:register` | `may_panic` | Shift left modulo the bit width after validating the count. |
| 72 | `integer_wrapping_shift_right` | `destination:register`, `left:register`, `right:register` | `may_panic` | Shift right under D035 after validating the count. |
| 73 | `integer_saturating_add` | `destination:register`, `left:register`, `right:register` | — | Add and clamp to the exact integer range. |
| 74 | `integer_saturating_subtract` | `destination:register`, `left:register`, `right:register` | — | Subtract and clamp to the exact integer range. |
| 75 | `integer_saturating_multiply` | `destination:register`, `left:register`, `right:register` | — | Multiply and clamp to the exact integer range. |
| 76 | `integer_saturating_divide` | `destination:register`, `left:register`, `right:register` | `may_panic` | Divide with D047 saturation and zero-divisor policy. |
| 77 | `integer_saturating_remainder` | `destination:register`, `left:register`, `right:register` | `may_panic` | Compute remainder with D047 saturation and zero-divisor policy. |
| 78 | `integer_saturating_negate` | `destination:register`, `source:register` | — | Negate and clamp to the exact integer range. |
| 79 | `integer_saturating_shift_left` | `destination:register`, `left:register`, `right:register` | `may_panic` | Shift left and clamp after validating the count. |
| 80 | `integer_saturating_shift_right` | `destination:register`, `left:register`, `right:register` | `may_panic` | Shift right under D035 after validating the count. |
| 81 | `integer_convert_total` | `destination:register`, `source:register` | — | Apply a range-preserving explicit integer conversion. |
| 82 | `integer_convert_option` | `destination:register`, `source:register` | `may_panic` | Return Some(converted) or None for a loss-detecting integer conversion. |
| 83 | `format_f32` | `destination:register`, `source:register` | `may_panic` | Format exact raw binary32 bits as the D046 string spelling. |
| 84 | `format_f64` | `destination:register`, `source:register` | `may_panic` | Format exact raw binary64 bits as the D046 string spelling. |
| 85 | `defer_register` | `marker:register`, `call:call` | — | Register one captured void call for lexical cleanup. |
| 86 | `defer_call` | `marker:register`, `call:call` | `may_panic` | Pop and invoke the most recently registered lexical cleanup. |
| 87 | `io_println` | `destination:register`, `text:register` | `may_panic` | Publish one D046 stdout line and return Result<void, std/io.Error>. |
| 88 | `test_output_begin` | `destination:register`, `expected:register` | `may_panic` | Begin exact-byte stdout capture for a compiler-known test expectation. |
| 89 | `test_output_end` | `destination:register`, `marker:register` | `may_panic` | End exact-byte stdout capture and panic with G4004 on mismatch. |
| 90 | `array_pop` | `destination:register`, `array:register` | `may_panic` | Remove and return the final initialized managed-array element. |
| 91 | `array_truncate` | `array:register`, `length:register` | `may_panic` | Remove initialized managed-array elements after one exact target length. |
| 92 | `array_clear` | `array:register` | `may_panic` | Remove every initialized managed-array element. |
| 93 | `array_reserve` | `array:register`, `additional:register` | `may_panic` | Reserve capacity for additional managed-array elements without changing length. |
| 94 | `array_generation` | `destination:register`, `array:register` | — | Read the exact managed-array mutation generation as u64. |
| 95 | `array_require_generation` | `destination:register`, `array:register`, `generation:register` | `may_panic` | Copy the array only if its mutation generation still matches the captured value. |
| 96 | `array_get_in_bounds` | `destination:register`, `array:register`, `index:register` | — | Load through a verifier-proven array/index token. |
| 97 | `array_set_in_bounds` | `array:register`, `index:register`, `value:register` | — | Store through a verifier-proven array/index token. |
| 98 | `i64_add_proven` | `destination:register`, `left:register`, `right:register` | — | Add i64 operands under a verifier-proven no-overflow interval fact. |
| 99 | `i64_subtract_proven` | `destination:register`, `left:register`, `right:register` | — | Subtract i64 operands under a verifier-proven no-overflow interval fact. |
| 100 | `i64_multiply_proven` | `destination:register`, `left:register`, `right:register` | — | Multiply i64 operands under a verifier-proven no-overflow interval fact. |
| 101 | `format_bool` | `destination:register`, `source:register` | `may_panic` | Format a boolean as its canonical lower-case string spelling. |
| 102 | `format_integer` | `destination:register`, `source:register` | `may_panic` | Format an exact-width integer as canonical base-ten text. |
| 103 | `string_concat` | `destination:register`, `left:register`, `right:register` | `may_panic` | Concatenate two counted immutable strings with bounded allocation. |
| 104 | `parse_integer` | `destination:register`, `text:register`, `radix:register` | `may_panic` | Parse counted ASCII integer text into Result<int, std/convert.ParseError>. |
| 105 | `io_write` | `destination:register`, `text:register`, `operation:i64` | `may_panic` | Publish exact string bytes to stdout or stderr under a typed CP32 stream operation. |
| 106 | `io_flush` | `destination:register`, `operation:i64` | `may_panic` | Flush exactly stdout or stderr under a typed CP32 stream operation. |
| 107 | `io_read_line` | `destination:register`, `limit:register`, `option_layout:i64` | `may_panic` | Read one bounded UTF-8 line from stdin into Result<Option<string>, std/io.Error>. |
| 108 | `text_byte_length` | `destination:register`, `text:register` | — | Return exact counted UTF-8 byte length. |
| 109 | `text_rune_count` | `destination:register`, `text:register` | — | Return the Unicode scalar count of valid UTF-8 text. |
| 110 | `text_grapheme_count` | `destination:register`, `text:register` | — | Return the D076 alpha grapheme count. |
| 111 | `text_is_rune_boundary` | `destination:register`, `text:register`, `offset:register` | — | Test one byte offset for a Unicode scalar boundary. |
| 112 | `text_drop_prefix` | `destination:register`, `text:register`, `offset:register` | `may_panic` | Copy text after one verified scalar-boundary byte offset. |
| 113 | `text_take_prefix` | `destination:register`, `text:register`, `offset:register` | `may_panic` | Copy text before one verified scalar-boundary byte offset. |
| 114 | `text_contains` | `destination:register`, `text:register`, `needle:register` | — | Test exact counted-byte containment. |
| 115 | `text_starts_with` | `destination:register`, `text:register`, `prefix:register` | — | Test an exact counted-byte prefix. |
| 116 | `text_ends_with` | `destination:register`, `text:register`, `suffix:register` | — | Test an exact counted-byte suffix. |
| 117 | `text_trim_ascii` | `destination:register`, `text:register` | `may_panic` | Copy text after D076 ASCII edge trimming. |
| 118 | `text_split_count` | `destination:register`, `text:register`, `separator:register` | — | Count D076 non-overlapping exact split parts. |
| 119 | `text_split` | `destination:register`, `text:register`, `separator:register` | `may_panic` | Construct owned D076 exact split parts. |
| 120 | `text_join` | `destination:register`, `parts:register`, `separator:register` | `may_panic` | Join an exact []string in one measured construction. |
| 121 | `text_runes` | `destination:register`, `text:register` | `may_panic` | Copy Unicode scalars into []rune. |
| 122 | `text_graphemes` | `destination:register`, `text:register` | `may_panic` | Copy D076 alpha graphemes into []string. |
| 123 | `text_encode_utf8` | `destination:register`, `text:register` | `may_panic` | Copy exact string bytes into []byte. |
| 124 | `text_invalid_utf8_offset` | `destination:register`, `bytes:register` | — | Return the first malformed UTF-8 byte offset or -1. |
| 125 | `text_decode_utf8` | `destination:register`, `bytes:register` | `may_panic` | Copy verified []byte into immutable string. |
| 126 | `io_read_bytes` | `destination:register`, `limit:register`, `array_layout:i64` | `may_panic` | Read bounded binary stdin through EOF into Result<[]byte, std/io.Error>. |
| 127 | `io_write_bytes` | `destination:register`, `bytes:register`, `operation:i64` | `may_panic` | Publish exact []byte data to stdout or stderr under D076. |
| 128 | `path_normalize` | `destination:register`, `text:register` | `may_panic` | Lexically normalize one bounded target-host path into Result<string, std/path.Error>. |
| 129 | `path_is_absolute` | `destination:register`, `text:register` | — | Test whether one normalized target-host path is absolute. |
| 130 | `fs_open_read` | `destination:register`, `path:register` | `may_panic` | Capability-check and open one symlink-free regular file for reading. |
| 131 | `fs_read` | `destination:register`, `handle:register`, `limit:register` | `may_panic` | Read one bounded open file through EOF without publishing a failed prefix. |
| 132 | `fs_close` | `destination:register`, `handle:register` | `may_panic` | Idempotently close one execution-local file handle with a typed result. |
| 133 | `fs_close_best_effort` | `destination:register`, `handle:register` | — | Idempotently attempt non-panicking cleanup of one execution-local file handle. |
| 134 | `fs_read_all` | `destination:register`, `path:register`, `limit:register` | `may_panic` | Capability-check and boundedly read one whole regular file. |
| 135 | `fs_write_atomic` | `destination:register`, `path:register`, `bytes:register` | `may_panic` | Capability-check and atomically publish one bounded byte array. |
| 136 | `fs_metadata` | `destination:register`, `path:register` | `may_panic` | Inspect bounded file or directory metadata without following links. |
| 137 | `fs_read_directory` | `destination:register`, `path:register`, `limit:register` | `may_panic` | Enumerate owned directory entries in deterministic UTF-8 order. |
| 138 | `env_get` | `destination:register`, `name:register` | `may_panic` | Read one explicitly granted environment name into Result<Option<string>, std/env.Error>. |
| 139 | `process_current_directory` | `destination:register` | `may_panic` | Return the captured absolute current directory as Result<string, std/process.ContextError>. |
| 140 | `process_executable_path` | `destination:register` | `may_panic` | Return the hosting process executable path as Result<string, std/process.ContextError>. |
| 141 | `collection_new` | `destination:register` | `may_panic` | Create one empty exact managed Map or Set value. |
| 142 | `collection_length` | `destination:register`, `collection:register` | — | Read the active entry count as i64. |
| 143 | `collection_contains` | `destination:register`, `collection:register`, `key:register` | — | Test exact key membership. |
| 144 | `collection_get` | `destination:register`, `collection:register`, `key:register` | — | Read a compiler-proven present Map value with defensive runtime validation. |
| 145 | `collection_put` | `collection:register`, `key:register`, `value:register` | `may_panic` | Insert or update one exact Map entry failure-atomically. |
| 146 | `collection_add` | `collection:register`, `key:register` | `may_panic` | Insert one absent exact Set element failure-atomically. |
| 147 | `collection_remove` | `destination:register`, `collection:register`, `key:register` | — | Remove one entry and report whether it existed. |
| 148 | `collection_clear` | `collection:register` | — | Remove every active entry. |
| 149 | `collection_generation` | `destination:register`, `collection:register` | — | Read the exact collection mutation generation as u64. |
| 150 | `collection_require_generation` | `destination:register`, `collection:register`, `generation:register` | `may_panic` | Copy a collection only when its iterator generation still matches. |
| 151 | `collection_entry_key` | `destination:register`, `collection:register`, `index:register` | — | Read an insertion-order key at a defensively checked active index. |
| 152 | `collection_entry_value` | `destination:register`, `collection:register`, `index:register` | — | Read an insertion-order Map value at a defensively checked active index. |
| 153 | `test_assert` | `destination:register`, `condition:register`, `message:register` | `may_panic` | Complete one compiler-known expectation or panic with G4004 at its source span. |
| 154 | `collection_get_option` | `destination:register`, `collection:register`, `key:register` | `may_panic` | Read one Map key into Option with exactly one collection lookup. |
| 155 | `collection_remove_option` | `destination:register`, `collection:register`, `key:register` | `may_panic` | Remove one Map key into Option with exactly one collection lookup. |
| 156 | `collection_put_result` | `destination:register`, `collection:register`, `key:register`, `value:register` | `may_panic` | Insert or update one Map entry and return its typed growth result after one lookup. |
| 157 | `collection_add_result` | `destination:register`, `collection:register`, `key:register` | `may_panic` | Insert one Set element and return its typed growth result after one lookup. |
| 158 | `collection_next` | `destination:register`, `collection:register`, `cursor:register` | — | Advance an independent checked storage cursor or return zero at exhaustion. |
| 159 | `collection_cursor_key` | `destination:register`, `collection:register`, `cursor:register` | — | Read the key at a checked positive active storage cursor. |
| 160 | `collection_cursor_value` | `destination:register`, `collection:register`, `cursor:register` | — | Read the Map value at a checked positive active storage cursor. |
| 161 | `array_copy` | `destination:register`, `array:register` | `may_panic` | Create a distinct exact-length shallow managed-array copy. |
| 162 | `array_remove_at` | `destination:register`, `array:register`, `index:register` | `may_panic` | Remove a checked array index stably and return the receiver after one atomic mutation. |
