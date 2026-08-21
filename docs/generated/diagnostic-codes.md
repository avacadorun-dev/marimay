# Ruda diagnostic code registry

<!-- Generated from bootstrap/tables/diagnostics.tsv. Do not edit by hand. -->

The committed registry is the source of stable diagnostic identities. Later gates add codes without reusing an existing code for a different rule.

| Code | Component | Meaning |
|---|---|---|
| `B0001` | run | Run pipeline could not lower, encode, decode, or verify canonical typed MIR. |
| `B0002` | build | Supported host C toolchain is unavailable. |
| `B0003` | build | Canonical typed MIR could not lower to portable C17. |
| `B0004` | build | Supported host C compiler rejected compiler-generated C. |
| `B0005` | build | Build cache or requested output could not be published atomically. |
| `B0006` | build | Host target cannot preserve required native semantics. |
| `B0007` | test | VM and native test observations differ. |
| `B0008` | build | Version-matched native runtime artifact is missing or incompatible. |
| `F0001` | formatter | Formatter could not read the selected source input. |
| `F0002` | formatter | Formatter could not write or transactionally replace canonical output. |
| `F0003` | formatter | Formatter exhausted a defensive input, nesting, work, or output limit. |
| `F0004` | formatter | Source input does not match canonical formatting. |
| `G4000` | runtime | Program executed an explicit source panic. |
| `G4001` | runtime | Checked integer arithmetic panicked. |
| `G4002` | runtime | Array indexing panicked on an out-of-bounds index. |
| `G4003` | runtime | Root main returned an expected error. |
| `G4004` | test | A compiler-known test expectation observed a mismatching value or byte stream. |
| `G4005` | runtime | An array iterator observed an intervening initialized-sequence mutation. |
| `I0001` | compiler | Compiler does not implement an accepted language construct. |
| `I0002` | runtime | Runtime could not complete because of an internal resource or integrity failure. |
| `K0001` | manifest | Manifest directive or grouping syntax is invalid. |
| `K0002` | manifest | Manifest contains an invalid JSON-compatible string. |
| `K0003` | manifest | Workspace use path is not a canonical relative path within the workspace root. |
| `K0004` | check | Checker could not discover or read the selected source target. |
| `K0005` | manifest | Module manifest does not provide one usable module and language identity. |
| `L0001` | lexer | Source contains malformed UTF-8. |
| `L0002` | lexer | Source contains a character that cannot begin a token. |
| `L0003` | lexer | Reserved word cannot be used as an identifier. |
| `L0004` | lexer | Nested block comment reaches end of source before closing. |
| `L0005` | lexer | Quoted literal reaches a physical newline or end of source before closing. |
| `L0006` | lexer | Quoted literal contains an invalid or incomplete escape. |
| `L0007` | lexer | Numeric literal does not follow the closed base, separator, or exponent grammar. |
| `L0008` | lexer | Rune literal does not contain exactly one Unicode scalar or escape. |
| `L0009` | lexer | Comment or interpolation nesting exceeds the configured defensive limit. |
| `L0010` | lexer | Token or diagnostic production exceeds the configured defensive limit. |
| `L0011` | lexer | Byte string contains non-ASCII text or a non-byte escape. |
| `L0012` | lexer | Interpolated text contains a single unmatched closing brace. |
| `P0001` | parser | Required syntax token is missing at the recovery point. |
| `P0002` | parser | Token is not valid in the current grammar position. |
| `P0003` | parser | Delimited syntax reaches end of source before closing. |
| `P0004` | parser | Parser nesting exceeds the configured defensive limit. |
| `P0005` | parser | Parser event or diagnostic production exceeds the configured defensive limit. |
| `P0006` | parser | Top-level declaration does not match a baseline declaration form. |
| `R0001` | resolver | Name does not resolve in the current lexical scope. |
| `R0002` | resolver | Name is declared more than once in the same lexical scope. |
| `R0003` | resolver | Public declaration signature exposes a package-private type. |
| `R0004` | resolver | Field name does not exist on the resolved nominal record or named enum payload. |
| `R0005` | resolver | Record or named enum payload construction supplies the same field more than once. |
| `R0006` | resolver | Variant name does not exist on the resolved nominal enum type. |
| `R0007` | resolver | Compiler-known attribute is invalid for the declaration or has invalid arguments. |
| `R0008` | resolver | Imported local package path does not resolve in the supplied package graph. |
| `R0009` | resolver | Local package imports contain a dependency cycle. |
| `R0010` | resolver | Named or namespace-qualified import does not name an exported declaration. |
| `R0011` | resolver | Imported declaration or member is package-private at the use site. |
| `R0012` | resolver | File-local import binding conflicts with another import or package declaration. |
| `T0001` | type checker | Type name does not resolve to a primitive, declared, or imported type. |
| `T0002` | type checker | Expression type is not compatible with the required exact type. |
| `T0003` | type checker | Operator is not defined for the operand types. |
| `T0004` | type checker | Assignment targets an immutable binding or record field. |
| `T0005` | type checker | Local binding is read before definite initialization. |
| `T0006` | type checker | Reachable path in a non-void function does not return a value. |
| `T0007` | type checker | Integer literal is outside the range of its required type. |
| `T0008` | type checker | Break or continue appears outside a loop. |
| `T0009` | type checker | Argument count does not match the resolved function or positional enum variant signature. |
| `T0010` | type checker | Required constant expression cannot be evaluated under constant semantics. |
| `T0011` | type checker | Type alias expansion contains a dependency cycle. |
| `T0012` | type checker | Record or named enum payload construction/pattern omits a required field. |
| `T0013` | type checker | Match over a closed domain does not cover every reachable case. |
| `T0014` | type checker | Match arm pattern is redundant or unreachable after earlier arms. |
| `T0015` | type checker | Yield appears outside a block match arm. |
| `T0016` | type checker | A reachable path through a block match arm does not yield a value. |
| `T0017` | run | Executable root does not declare an accepted main entry point. |
| `U0001` | command line | Unknown top-level command or help target. |
| `U0002` | command line | Unknown command-line option. |
| `U0003` | command line | Specified command is not present in this bootstrap stage. |
| `U0004` | command line | Command-line option is missing its required value. |
| `U0005` | command line | Command-line option value is outside its closed value set. |
| `U0006` | command line | Command received an unsupported positional argument. |
