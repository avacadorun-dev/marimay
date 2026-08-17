# Generated Ruda bytecode opcode registry

<!-- Generated from bootstrap/tables/opcodes.tsv. Do not edit by hand. -->

This registry assigns explicit stable wire identifiers to the currently
implemented internal typed-bytecode foundation. It does not by itself define a
complete `.rudab` bundle, VM execution support, or a public compatibility
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
| 11 | `i64_equal` | `destination:register`, `left:register`, `right:register` | — | Compare signed 64-bit operands for equality. |
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
