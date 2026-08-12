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
