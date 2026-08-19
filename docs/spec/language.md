# Ruda language specification — design baseline 0.1

Status: normative design baseline.  
Language version: `0.1-design`.

This document defines the source language. Runtime and engine rules are defined in [`execution.md`](execution.md); command behavior is defined in [`toolchain.md`](toolchain.md).

The key words **must**, **must not**, **should**, and **may** are normative. Anything explicitly marked provisional requires its roadmap gate before the 1.0 syntax freeze.

## 1. Program model

A Ruda program consists of UTF-8 `.ruda` files grouped into packages by directory. A root `ruda.mod` gives the module path and language version. All files in one directory contribute declarations to one package namespace. Imports are file-local. Package dependency cycles are compile errors.

Safe source is statically typed before either VM or native execution. There is no dynamically typed execution tier and no unchecked fallback after a type error.

The valid entry points are:

```ruda
fn main(): void;
fn main(): Result<void, E>;
```

Under D049, `E` satisfies the preview structural `Error` contract when its
root-visible D003 dot-call set contains an exact non-external
`message(error: E): string`. Returning `Err` writes `G4003` with those exact
counted message bytes to stderr and exits with status `1`. A panic exits
according to the runtime panic policy and is not an expected-error path.

User program stdout is not a language primitive. The 0.1-dev write surface is
`std/io.println(text: string): Result<void, io.Error>` under D046. It is not in
the prelude, does not accept a non-`string` argument, and is not the internal
`ok:f64:` engine-test protocol.

## 2. Lexical structure

### 2.1 Encoding and source positions

Source is valid UTF-8. A byte-order mark is accepted only at byte zero and is ignored. Diagnostics identify physical byte offset, one-based line, and one-based Unicode-scalar column. Physical source bytes remain unchanged. Under D022, the accepted byte-zero BOM does not advance the logical column; LF, CRLF, and lone CR each form one logical newline. An offset on a newline byte maps to the preceding line's end position, and the offset after the complete sequence maps to column 1 of the next line.

Under D023, identifiers use Unicode 17.0.0 and the UAX #31 R1-1 form `XID_Start XID_Continue*`. Public APIs should use ASCII identifiers. Identifiers are case-sensitive and comparisons use exact source code points; the compiler does not normalize spelling and warns about identifiers that differ only by Unicode normalization. A standalone ASCII `_` is the wildcard/discard token, not an identifier. `_name` is therefore invalid, while `name_2` is valid because underscore and decimal digits are `XID_Continue` characters.

### 2.2 Comments

`//` starts a line comment. `/* ... */` is a nestable block comment. Documentation comments are `///` for the next declaration and `/** ... */` for block documentation. The formatter preserves documentation attachment and never silently converts comment kind.

### 2.3 Hard keywords, contextual words, and reserved sentinels

The 0.1 hard keywords are unavailable as identifiers in every source context:

```text
defer false if match return spawn true while
```

The following words are contextual. They retain their grammatical role only
where that role is unambiguous and otherwise become ordinary identifiers in
the syntax tree, including declaration names, field/member names, parameters,
locals, and call expressions:

```text
as break case const continue else enum extern fn for from import interface let
mutable of public scope type unsafe yield
```

In particular, `yield expression;` is the contextual match-arm transfer form,
while `yield()`, `task.yield()`, and a declaration named `yield` use the ordinary
identifier. The words `test`, `fuzz`, `panic`, `in`, `any`, `async`, `await`,
`class`, `extends`, `impl`, and `throws` are ordinary identifiers. The absence
of nullability, source async coloring, classes, inheritance, `impl` blocks,
exception syntax, and an `any` escape type is enforced by grammar and types;
it is not implemented by occupying those useful API names.

Contextual construct recognition is bounded and deterministic. `break;` and
`continue;` are loop-control statements; call, member, postfix, and declaration
uses of those spellings are names. `const`/`let` introduce bindings only before
a binding name or `_`; `for` introduces the specified loop headers; and
`scope`/`unsafe` introduce statement blocks at a statement boundary and remain
names in an already established expression context. At top level the declaration
starters keep their roles at a declaration boundary. An `else` attaches to an
`if` only before another `if` or a block. Recovery accepts the incomplete
prefixes needed for targeted diagnostics without changing valid name uses.

Only `null` and `undefined` are reserved sentinels. They produce targeted
diagnostics and cannot act as identifiers, so the language cannot accidentally
acquire null-like value spellings through ordinary name resolution.

### 2.4 Semicolons

There is no automatic semicolon insertion. A semicolon terminates simple statements, field signatures, interface call signatures, type aliases, and imports. Record, function, interface, enum, control-flow, and `scope` blocks do not receive a trailing semicolon unless the whole block is used as a subexpression requiring one.

### 2.5 Literals

- booleans: `true`, `false`;
- decimal integers: `12`, `1_000_000`;
- binary/octal/hex integers: `0b1010`, `0o755`, `0xff`;
- floating point: `1.0`, `6.02e23`;
- strings: `"UTF-8 text"`;
- raw strings: `r"no escapes\n"` with hash-delimited forms reserved for 0.2;
- byte strings: `b"ASCII or escaped bytes"`;
- interpolated strings: `$"user {user.id}"`;
- runes: `'λ'`.

Under D024, digits in source literals are ASCII and digit separators must occur singly between digits valid for that base. Integer base prefixes are `0b`/`0B`, `0o`/`0O`, and `0x`/`0X`. Decimal floats are either decimal digits, `.`, decimal digits, with an optional exponent, or decimal digits followed by an exponent. An exponent is `e`/`E`, an optional `+`/`-`, and one or more decimal digits. Hexadecimal floats, leading-dot floats, and numeric suffixes are not accepted in 0.1.

Normal strings and runes accept `\\`, `\"`, `\'`, `\n`, `\r`, `\t`, `\0`, and `\u{H...}` with one to six hexadecimal digits naming a Unicode scalar value. Byte strings accept the simple escapes and `\xHH` with exactly two hexadecimal digits, and otherwise contain ASCII bytes only. Raw strings do not process escapes. An unescaped physical newline terminates none of these literal forms, and a rune contains exactly one scalar or escape.

String indexing by integer is forbidden because it is ambiguous between bytes and Unicode scalars. Use explicit byte, rune, or grapheme APIs.

An interpolated string starts with `$"` and ends with `"`. `{expression}` inserts the standard string representation of an expression; `{{` and `}}` produce literal braces. Interpolations are parsed and typed as ordinary expressions, evaluated left to right exactly once, and cannot contain statements. That interpolation representation is not the D046 stdout contract and is not an implicit conversion into `println`; a non-`string` interpolation result cannot be passed to `println` until a later decision specifies the interpolation type rule.

Interpolated text uses the normal-string escape set. The lossless token stream retains the `$"`, text, expression-boundary braces, and final `"` separately. A single `}` in text is invalid; nested strings and balanced expression braces are permitted within the implementation's documented defensive nesting limit.

### 2.6 Compiler attributes

`@` introduces a compiler-known attribute. The 1.0 set is closed:
`@derive(...)`, `@link(...)`, `@blocking`, `@test`, and `@fuzz` are defined by
this specification. `@test` and `@fuzz` take no arguments, cannot be combined,
and mark ordinary package-private functions in files ending `_test.ruda`;
`@test` requires
`fn(): void`, while `@fuzz` requires exactly one typed parameter and `void`.
Unknown or misplaced attributes are errors. Attributes cannot execute code,
inspect arbitrary compiler state, or rewrite syntax.

## 3. Packages, imports, and public API

Named imports and namespace imports are supported:

```ruda
import { Decoder, decode } from "std/json";
import { User as StoredUser } from "acme/store";
import * as http from "std/http";
```

There are no default imports and no wildcard injection into the local namespace. Import paths identify packages, not individual files. Relative paths are allowed only within the same root module and are normalized by `ruda fmt`.

Top-level declarations are package-private unless prefixed with `public`. The same visibility word applies to declarations and record fields:

```ruda
public type User {
  public id: int;
  public name: string;
}

public fn findUser(id: int): Result<User, StoreError> {
  // ...
}
```

Record fields are package-private unless marked `public`. Interface call signatures are public by definition. A public declaration must not expose a package-private type in its signature.

A type in a public signature crosses a package boundary as the same canonical,
fully qualified type after aliases are resolved. Generic arguments are part of
that identity. In particular, `Array<T>` in an imported parameter or result
retains the exact canonical `T` and its recursive managed layout; the caller
and callee do not insert package-specific array or element conversions. A
public alias may expose another spelling of that same type, and a public
wrapper may forward it, but an import does not itself re-export a name.

Importing a package does not execute initialization code. Top-level values are immutable `const` declarations whose initializers must be compile-time constant expressions. Runtime initialization is explicit in functions.

## 4. Bindings and assignment

`const` creates a non-rebindable local. `let` creates a rebindable local:

```ruda
const port: int = 8080;
let attempts = 0;
attempts += 1;
```

Every local must be definitely initialized before use. Shadowing a local in the same block is an error. Shadowing in a nested lexical block is allowed but warned by default and rejected by `ruda vet --strict`.

`const` controls the binding, not deep object mutability. A `const` reference may modify a field declared `mutable`. This distinction must be stated by IDE hover information.

Assignments evaluate the target location first and the value second, each exactly once. Chained assignment is not supported.

## 5. Types

### 5.1 Primitive types

| Type | Meaning |
|---|---|
| `bool` | `true` or `false` only |
| `i8`, `i16`, `i32`, `i64` | signed two's-complement integers |
| `u8`, `u16`, `u32`, `u64` | unsigned integers |
| `int` | exact alias of `i64` |
| `uint` | exact alias of `u64` |
| `f32`, `f64` | IEEE 754 binary floating point |
| `float` | exact alias of `f64` |
| `byte` | exact alias of `u8` |
| `rune` | a valid Unicode scalar value |
| `string` | immutable UTF-8 text |
| `void` | successful absence of a value |
| `never` | an uninhabited result of an expression that cannot complete normally |

Integer overflow traps in all build modes. Explicit operations such as `wrappingAdd`, `saturatingAdd`, and `checkedAdd` request other behavior. Integer division and remainder by zero and an out-of-range shift panic. Narrowing and signedness-changing integer conversions are explicit and return `Option` where information may be lost.

Under D047 the compiler-known prelude exposes checked, wrapping, and
saturating `Add`, `Subtract`, `Multiply`, `Divide`, `Remainder`, `Negate`,
`ShiftLeft`, and `ShiftRight` families for the eight integer primitives.
Checked functions return `Option<T>` for overflow and invalid operands;
wrapping and saturating functions return `T` and retain the ordinary panic for
zero divisors and invalid shift counts. Exact signatures and the `MIN / -1`
outcomes are defined by D047.

An explicit integer conversion is `value as Target`. When the entire source
range fits in `Target`, the expression has type `Target`; otherwise it has type
`Option<Target>` and returns `None` for an unrepresentable runtime value. The
form does not define integer/float conversion and never wraps or saturates.
`as` binds between prefix unary and multiplicative operators and associates
left to right.

Signed `/` rounds its mathematical quotient toward zero. Signed `%` is
defined by `left - (left / right) * right`; a nonzero remainder has the sign of
`left` and magnitude less than `abs(right)`. For a signed type, both
`MIN / -1` and `MIN % -1` panic as checked overflow. Unsigned `/` and `%` use
the nonnegative quotient and remainder. These rules apply before any host
backend operator is selected.

Shift operands have the same exact integer type and the result retains that
type. The count must satisfy `0 <= count < bitWidth`; a negative signed count
or a count at least equal to the width panics. Left shift is checked
multiplication by a power of two and panics when the mathematical result is
not representable; high bits are not silently discarded. Unsigned right shift
fills with zero. Signed right shift copies the sign bit using the language's
two's-complement value model. Explicit wrapping shifts are numeric-library
operations, not alternate source operators.

`f64` has the deterministic binary64 contract in D044. Decimal literals round
their mathematical decimal value to binary64 with round-to-nearest,
ties-to-even, independent of locale. Constants, copies, call arguments, call
results, and returns preserve raw binary64 bits. Arithmetic rounds at binary64
precision using round-to-nearest, ties-to-even and canonicalizes every NaN to
`0x7ff8000000000000`; unary `-` toggles the sign bit exactly and does not
canonicalize a NaN. `f64` division by positive or negative zero follows IEEE
754 and does not panic. Equality and ordered comparisons use IEEE rules: the
two zero signs compare equal, and a NaN is unequal and unordered.

Floating-point operations use strict semantics. Optimizations must not
reassociate expressions, discard NaN behavior, contract operations, use excess
precision, or enable a global fast-math dialect. Numerically relaxed algorithms
use explicit library APIs. `f32` literal conversion, NaN canonicalization, and
backend conformance are not yet specified by D044 and must not be inferred from
the `f64` rules.

Conditions require `bool`; there is no truthiness.

### 5.2 Standard generic types

The prelude defines at least:

```ruda
enum Option<T> {
  Some(T),
  None,
}

enum Result<T, E> {
  Ok(T),
  Err(E),
}

interface Error {
  message(): string;
}
```

Any nominal type whose dot-call set contains an exact visible `message(): string` signature can be used as `Error`. The interface does not prescribe logging, status codes, causes, or stack capture; packages model those through richer interfaces or enum payloads.

When a positional enum variant's instantiated payload type is `void`, its constructor takes no argument. Thus a function returning `Result<void,E>` completes with `Ok()`.

`Array<T>` is an ordered mutable managed-reference collection under D045.
`Map<K,V>` preserves insertion iteration order and requires keys supporting
equality and hashing. Updating an existing key retains its position; deleting
and reinserting appends it. `Set<T>` follows the same order rule. `Slice<T>` is
a non-owning, scope-bounded view supplied by compiler-known library types; it
cannot outlive its owner and does not expose lifetime syntax.

There is no null-like value. A reference, interface value, array, map, function value, and string is always valid. Absence is represented by `Option<T>`.

### 5.3 Type aliases

```ruda
type UserId = i64;
```

A scalar or compound alias creates another spelling of the same type. The equals sign distinguishes aliases from nominal record declarations.

### 5.4 Nominal records

```ruda
public type Account {
  public id: int;
  public owner: string;
  mutable balance: int;
}
```

A record declaration creates a nominal managed-reference type. Two record declarations with identical fields are distinct. Fields are immutable after construction unless marked `mutable`. `public mutable` is permitted; unqualified mutable fields remain package-private.

Construction names the nominal type and supplies every field exactly once:

```ruda
const account = Account {
  id: 42,
  owner: "Mara",
  balance: 100,
};
```

Field names select their destination, but source order is semantically
significant for evaluation. Initializers are evaluated exactly once from left
to right in the order written, even when that differs from declaration order.
The formatter must preserve this order and must not require name resolution to
format a construction. Unknown, duplicate, and missing fields are errors.
There are no implicit default values.

Inside a named record or enum-payload construction, `{ id }` is shorthand for `{ id: id }` when a visible local named `id` exists with the required type.

Record values have reference identity and are garbage collected. The compiler may stack-allocate or scalar-replace a record only when identity and lifetime observations remain unchanged. Identity comparison uses the prelude operation `identity.same(a, b)`. It accepts exactly two values of the same nominal record type after aliases are resolved, evaluates them once from left to right, and returns `true` exactly when both values denote the same managed record. Equal fields, mutation, object relocation, and engine-specific addresses do not affect this result. Strings, enums, primitive values, and two distinct nominal record types are not accepted by this operation. The prelude name `identity` follows ordinary lexical shadowing; it is not a keyword. Ordinary `==` is available only when the record has a valid compiler-derived or explicitly declared equality function in its dot-call set.

The closed compiler derive set is written before a declaration:

```ruda
@derive(Debug, Eq, Hash)
type Coordinate {
  public x: int;
  public y: int;
}
```

`Eq` and `Hash` are rejected for records containing mutable fields. A derive expands to ordinary typed HIR before interface checking and has no arbitrary user code execution.

The prelude equality contract is `equal(other: T): bool`. `==` and `!=` are compiler-bound to that exact dot-call signature for types that satisfy the contract; this is the only user-type operator hook in 1.0 and does not permit arbitrary operator declarations. `@derive(Eq)` generates an ordinary eligible `equal(value, other)` function; `@derive(Hash)` generates the matching stable value hash contract. Reference identity remains the separate `identity.same` operation.

### 5.5 Algebraic enums

Enums are closed nominal sums:

```ruda
enum Message {
  Quit,
  Write(string),
  Move { x: int; y: int },
}
```

The three forms are unit, positional, and named payload variants. Construction is qualified:

```ruda
const a = Message.Quit;
const b = Message.Write("hello");
const c = Message.Move { x: 10, y: 20 };
```

Under D025, named payload declarations use semicolon-terminated field signatures, as in `Move { x: int; y: int; }`; the optional comma after the closing brace separates enum variants. Construction and pattern fields remain comma-separated values or bindings.

Prelude constructors `Some`, `None`, `Ok`, and `Err` are available unqualified. All other variants are qualified outside a pattern. The compiler owns tag layout; safe code cannot inspect or forge the integer tag.

Enums are immutable. Adding, removing, renaming, or changing a public variant is a breaking package API change. There is no open enum in 1.0.

### 5.6 Tuples and function types

Tuple types use brackets in type position and values use brackets with a fixed arity:

```ruda
const pair: [string, int] = ["port", 8080];
```

`Array<T>` construction uses the compiler-known variadic prelude
`arrayOf(...)` function or a contextual array literal. This avoids inventing
static methods on types and avoids treating arbitrary heterogeneous values as
an array. Arguments evaluate once from left to right and require one exact
element type. `arrayOf<T>()` constructs an empty array with an explicit element
type. An empty literal requires an expected `Array<T>` type; a non-empty
literal infers one exact common element type and performs no implicit numeric
or user conversion.

The prelude exposes ordinary free functions whose exact first `Array<T>`
parameter makes them dot-call eligible:

```ruda
public fn length<T>(values: Array<T>): int;
public fn push<T>(values: Array<T>, value: T): Result<void, ArrayGrowthError>;

public enum ArrayGrowthError {
  LengthLimit { limit: int; },
}
```

The variadic behavior of `arrayOf` is compiler-known and does not introduce
user-declarable variadic functions. `length(values)` and `values.length()` are
the same call. `push` returns `Err(ArrayGrowthError.LengthLimit { limit })`
only when adding an element would exceed the semantic maximum `int::MAX`.
Capacity is unobservable. Runtime heap ceilings and process allocation failure
remain execution outcomes rather than recoverable array values.

Function types use arrow notation:

```ruda
type Predicate<T> = (T) => bool;
type Mapper<T, U> = (T, int) => U;
```

Parameter types are invariant for interface and function compatibility in 1.0. Return types must be identical except that `never` is compatible with every return position. Function parameters do not use bivariant compatibility.

### 5.7 Generics

Generic parameters are declared with angle brackets and may have interface constraints:

```ruda
fn first<T>(values: Array<T>): Option<T> {
  // ...
}

fn maximum<T: Ordered<T>>(values: Array<T>): Option<T> {
  // ...
}
```

Multiple constraints use `&`. Generic arguments are invariant unless a future language version adds explicit variance. 1.0 has no higher-kinded types, const generics, associated types, specialization declarations, generic parameter defaults, or user-visible lifetime parameters.

Type arguments are inferred from value arguments and expected return context when the solution is unique. Public function parameters and returns remain explicitly annotated.

### 5.8 Interfaces

An interface is a structural contract containing dot-call signatures only:

```ruda
public interface Stringer {
  string(): string;
}

interface Reader {
  read(buffer: Buffer): Result<int, IoError>;
  close(): Result<void, IoError>;
}
```

A nominal type satisfies an interface when its visible dot-call set contains every required signature with the exact generic arity, remaining parameter modes, remaining parameter types, and return type. No declaration names the conformance.

Interfaces have no fields, constructors, static requirements, associated types, default bodies, or negative implementations. Interface embedding is syntactic signature inclusion:

```ruda
interface ReadWriter {
  Reader;
  Writer;
}
```

Conflicting embedded signatures are compile errors. Interface values carry a value reference plus a compiler-generated method table. An interface value is never null. Matching an interface value requires a wildcard because its implementation set is open.

## 6. Functions, dot calls, and closures

### 6.1 Ordinary functions

```ruda
fn parsePort(text: string): Result<int, ParseError> {
  // ...
}
```

Every named function declares parameter and return types. Arguments are evaluated left to right. Default arguments and named call arguments are not supported in 1.0. Ordinary functions do not overload by parameter type.

`return expression;` returns from the nearest named function or closure.
`return;` is valid only for `void`. All reachable paths of a function whose
result is neither `void` nor `never` must return a value.

A function may explicitly declare `never` as its result. Every reachable path
of that body must terminate without a normal return value; falling through,
`return;`, and returning an ordinary value are errors. Evaluating a call whose
resolved signature returns `never` first evaluates every argument left to right
and then has no normal continuation. The call expression has type `never`,
which is accepted in any result position because no value reaches that
position. Implementations do not create a runtime value of type `never`.

### 6.2 First-parameter dot calls

An ordinary package-level function is dot-call eligible when its first parameter has an exact nominal type owned by the declaring package:

```ruda
public fn string(user: User): string {
  return $"{user.name} (#{user.id})";
}
```

The following calls resolve to the same symbol. The subject expression in dot form is evaluated exactly once and becomes the first argument:

```ruda
const a = user.string();
const b = string(user);
```

Rules:

- an eligible function is top-level and may appear in any file of the owning package;
- its first parameter uses an ordinary descriptive name such as `user`, `order`, or `value`;
- its first parameter type must exactly match a nominal or compiler-known library type owned by the declaring package; `string`, collections, task/channel handles, and opaque resources each have one designated standard owning package, while aliases, interfaces, and numeric/boolean scalars do not create ownership or eligibility;
- a generic wrapper, interface type, structural shape, or foreign nominal type cannot be used to add dot-call behavior;
- the same function name may be declared for distinct locally owned first nominal parameter types; no other overloads are allowed;
- dot lookup uses the subject's exact static nominal type or an interface value's method table;
- whenever a public nominal type from an imported owning package is usable,
  every public eligible function owned by that package participates in its
  dot-call set, whether the type arrived through a named type import, the
  function through a named import, or the package through a namespace import;
- import form changes only free-call spelling: a named function import permits
  `pathParameter(request, "id")`, a namespace import permits
  `http.pathParameter(request, "id")`, and a type-only named import may expose
  neither free spelling while still permitting `request.pathParameter("id")`;
- structural interface conformance therefore does not change merely because
  the same owning package was imported with a different form;
- imported packages cannot retroactively add dot-call behavior to foreign types;
- taking the value of a same-name overloaded function requires qualification or an expected function type that selects one declaration;
- there is no `impl` declaration.

A dot-call-eligible function can mutate only fields declared `mutable` and visible to its package. Mutation does not require a special parameter mode because tasks sharing an isolate cannot execute in parallel. Cross-isolate sharing rules are defined in the execution specification.

### 6.3 Closures

```ruda
const doubled = values.map((value: int): int => value * 2);

const handler = (request: Request): Response => {
  return route(request);
};
```

Closures capture lexical `const` bindings by value and `let` bindings through a managed mutable cell. Each iteration of a `for` loop creates a fresh iteration binding. The compiler warns when a closure captures a mutable local and escapes the declaring block.

Suspension inference applies through closures exactly as through named functions; no `async` closure syntax exists.

## 7. Expressions and evaluation

Operand and argument evaluation is strictly left to right. Every source subexpression is evaluated at most once unless explicitly repeated in source. This order is identical in VM and native builds.

Operator precedence, highest to lowest:

1. member access, calls, indexing, postfix `?`;
2. unary `!`, unary `-`, bitwise `~`;
3. `*`, `/`, `%`;
4. `+`, `-`;
5. shifts;
6. comparisons `< <= > >=`;
7. equality `== !=`;
8. bitwise `& ^ |`;
9. logical `&&`;
10. logical `||`;
11. assignment operators.

`&&` and `||` short-circuit. `<<` and `>>` use the exact checked semantics in
section 5.1; `<<=` and `>>=` perform the same operation before assignment.
Comparison chaining is not supported. User-defined implicit conversions and
general operator overloading are absent in 1.0. Compiler-known derives may
provide equality and hashing under their fixed contracts.

An array index has exact type `int`/`i64`; unsigned and other integer widths do
not convert implicitly. Access is valid exactly when
`0 <= index < array.length()`. Every access performs this check in every build
mode; safe code cannot suppress it, though a proven redundant check may be
removed by optimization. A failed read or write produces the D045 `G4002`
panic before accessing element storage.

For `array[index] = value`, evaluation proceeds as receiver, index, bounds
check, right-hand side, then store. For a compound indexed assignment, the old
element is also read after the bounds check and before the right-hand side.
Each source expression is evaluated once. A failing bounds check performs no
write and does not evaluate the right-hand side.

## 8. Control flow

Conditions omit mandatory parentheses and require `bool`:

```ruda
if ready {
  start();
} else {
  wait();
}

while queue.notEmpty() {
  process(queue.removeFirst());
}
```

Iterable and C-style loops are supported:

```ruda
for const item of items {
  process(item);
}

for let index = 0; index < count; index += 1 {
  visit(index);
}
```

These are the two complete 0.1 header forms. A for-of header is exactly
`const` followed by an identifier, `of`, and an expression. A C-style header is
exactly `let` followed by an identifier, `=`, and three required expressions
separated by semicolons. It does not admit an arbitrary initializer, `const`,
an `_` binding, or an omitted initializer, condition, or step; the for-of form
does not admit `let`. D033 fixes this source grammar without changing the
existing iterator-cleanup or control-flow semantics.

`break;` and `continue;` target the nearest loop. Either statement outside a
loop is a compile error. Labels are not supported in 1.0. A `for-of` loop
obtains an iterator once and guarantees its cleanup through the standard
resource protocol.

## 9. Pattern matching

`match` is an expression. The scrutinee is written directly before its block:

```ruda
fn describe(value: Result<User, LookupError>): string {
  return match value {
    case Ok(user) => user.string();
    case Err(NotFound) => "missing";
    case Err(Denied { reason }) => $"denied: {reason}";
  };
}
```

Patterns include:

- wildcard: `_`;
- new binding: `value`;
- literals: `0`, `"ready"`, `true`;
- enum variants: `None`, `Some(value)`, `Move { x, y }`;
- records: `User { id, name }`;
- tuples: `[name, count]`;
- alternatives: `NotFound | Gone`;
- nested forms of the above.

A record pattern is nominal: its constructor must resolve to the exact record
type of the scrutinee, or to an alias of that type. Its named shape lists every
declared field exactly once; field order is irrelevant, `field` is shorthand
for `field: field`, and unknown, duplicate, or missing fields are errors. A
field-complete record pattern containing only bindings and wildcards is
irrefutable because a record type has one constructor. Consequently an
unguarded such arm covers the record domain and makes later arms redundant;
a guarded arm does not contribute to coverage. Nested/refutable field patterns
follow the ordinary recursive pattern rules.

String literal patterns compare the exact decoded UTF-8 byte sequence,
including embedded zero bytes. Ordinary and raw spellings that decode to the
same bytes denote the same pattern. Matching does not apply locale collation,
case folding, or Unicode normalization.

A guard follows the pattern:

```ruda
case Ok(value) if value > 0 => value;
```

Every alternative in an OR pattern must bind the same names with identical types. A guard is evaluated only after its pattern matches and does not contribute to exhaustiveness because it may be false.

A multi-statement arm uses the contextual `yield expression;` form for the
match value:

```ruda
const label = match status {
  case Ready => "ready";
  case Failed(error) => {
    log.warn(error);
    yield "failed";
  }
};
```

Every reachable path of a block arm must `yield`, `return`, `panic`, or otherwise have type `never`. A `return` exits the enclosing function, not merely the arm.

Outside a block match arm, the same statement-shaped form is rejected as a
misplaced match transfer. Call-shaped `yield()` and member-shaped
`task.yield()` remain ordinary name resolution and do not transfer a match
value.

A match over a closed enum or finite boolean domain must be exhaustive. Numeric
and string domains are not finite, so literal arms alone cannot exhaust them;
an unguarded wildcard or name binding must cover the remainder. Missing cases
are an error listing uncovered witnesses. Redundant and unreachable arms are
errors by default. A wildcard covers remaining cases but is discouraged by
`ruda vet` when all variants are locally visible.

All non-`never` arms must have an identical result type in 1.0; the compiler does not infer ad-hoc unions.

## 10. Expected errors, panic, and cleanup

### 10.1 `Result` and postfix `?`

Postfix `?` is valid only on `Result<T,E>` inside a function or closure returning `Result<U,E>`:

```ruda
fn configError(error: fs.Error): ConfigError {
  return ConfigError.Read(error);
}

fn readConfig(filePath: path.Path, limit: int): Result<Config, ConfigError> {
  const text = fs.readText(filePath, limit).mapError(configError)?;
  return decodeConfig(text);
}
```

If the operand is `Ok(value)`, the expression evaluates to `value`. If it is `Err(error)`, control returns `Err(error)` from the enclosing function after running required cleanup. The error type must be identical; conversion uses an explicit `mapError` or match.

This exactness remains subject to the E0 ergonomics gate, not to silent implementation drift. The pilot records explicit mapping sites across at least three multi-layer domain boundaries and measures whether locally visible conversion remains readable. If the evidence rejects the current balance, D006 and this section must be amended with syntax/type/checker/VM/native tests before I10 begins; the compiler cannot introduce an implicit conversion as a convenience shortcut.

In the example, omitting `.mapError(configError)` is a compile error because
`fs.Error` and `ConfigError` are distinct, and omitting `limit` is a call-arity
error. The documentation corpus contains matching compile-pass and compile-fail
obligations; until the `std/fs` surface is present in the I2 checker they are
parser-backed and cannot be reported as type-check evidence.

The operator does not catch panic, cancellation, or an engine fault. It is deliberately visible at the propagation site.

### 10.2 Panic

The prelude declares ordinary intrinsic function
`fn panic(message: string): never`. Calling the unshadowed prelude name
indicates an invariant violation and produces stable runtime
code `G4000`; the message is the exact runtime string value and the primary
source span covers the complete call expression. Bounds failure, checked
arithmetic failure, invalid internal runtime state, and failed explicit
assertions panic with their own stable codes. Panics run `defer` cleanup, cancel
structured child tasks, and produce a source-oriented stack trace. Safe Ruda
has no general `catch`; the test runtime can assert panic behavior in an
isolated test task.

`panic` is not a keyword or a special call grammar. An ordinary lexical or
package declaration named `panic` shadows the prelude intrinsic under normal
name-resolution rules; calls to that declaration have its declared type and
behavior. The intrinsic lowering applies only after resolving the unshadowed
prelude symbol.

### 10.3 `defer`

`defer` schedules a call for the exit of the current lexical block:

```ruda
{
  const file = fs.open(path, options)?;
  defer file.closeBestEffort();

  process(file)?;
  file.close().mapError(processCloseError)?;
}
```

The explicit `close()` on the success path propagates a cleanup failure after
domain mapping. The deferred idempotent `closeBestEffort()` is the fallback for
every earlier exit. It is an ordinary standard-library function, not special
`defer` syntax; its contract is to attempt release without panicking or
replacing the in-flight fallthrough, `return`, expected `Err`, panic, or
cancellation outcome. Code that must observe a failure on every exit defers an
ordinary `void` helper that matches `close()` and reports the error to an
explicit application sink.

Deferred calls run exactly once in last-in-first-out order on normal
fallthrough, `return`, `?`, `break`, `continue`, panic, and task cancellation.
The callee and arguments are evaluated when the `defer` statement executes and
are retained until cleanup. A deferred call must return `void`; `Result` is
never implicitly discarded or unwrapped. An already-requested cancellation is
masked while the LIFO cleanup list runs.

If an arbitrary deferred `void` call panics, later LIFO cleanups are still
attempted. That first cleanup panic replaces fallthrough, a returned value,
expected `Err`, loop transfer, or cancellation. It does not replace an
already-active panic; cleanup panics are then attached to the original as
suppressed failures in observation order.

Lexical rather than function-wide lifetime prevents loop iterations from accumulating resources unintentionally.

## 11. Structured tasks in source

There is no `async` or `await`. A call to a suspending operation retains its declared result type and may park only the current lightweight task. Concurrency is created explicitly:

```ruda
fn dashboard(id: int): Result<Dashboard, Error> {
  scope {
    const userTask = spawn loadUser(id);
    const ordersTask = spawn loadOrders(id);

    const user = userTask.wait()?;
    const orders = ordersTask.wait()?;

    return Ok(Dashboard { user, orders });
  }
}
```

`spawn expression` is legal only while lexically nested in an active `scope` block and returns a scope-bound `Task<T>`. `wait()` suspends the parent until the child completes and returns the child's `T`. Task handles are non-copyable compiler-known values and cannot escape, be stored, captured by an escaping closure, or cross an isolate.

Leaving `scope` by fallthrough, `return`, `?`, panic, or cancellation first requests cancellation of unfinished children and joins them. Detailed scheduling, failure, and memory rules are normative in [`execution.md`](execution.md).

## 12. Unsafe code and C interoperation

Safe Ruda has no raw pointer type in the prelude. C-compatible types and pointers live in `std/c`. An extern declaration names its ABI:

```ruda
import * as c from "std/c";

@link("sqlite3")
extern "c" fn sqlite3_open(
  filename: c.CString,
  database: c.OutPointer<c.Opaque>,
): c.int;
```

Calling an extern function, dereferencing a raw pointer, constructing a pointer from an integer, or asserting a layout is permitted only inside `unsafe { ... }` or an `unsafe fn`.

`unsafe` removes specific compiler checks; it does not disable typing, bounds checks unrelated to the operation, task structure, or engine equivalence. Every public unsafe function must have a documentation section titled `Safety` listing caller obligations.

An extern known to block is marked `@blocking`; the runtime invokes it on its bounded blocking pool so the isolate scheduler can continue. An unmarked extern that blocks violates its declared contract. C callbacks into a suspending Ruda function are forbidden; callback entry points must be non-suspending, which the compiler verifies from the inferred effect graph.

## 13. Constant evaluation

Constant expressions include literals, immutable enum construction from constant payloads, arithmetic that cannot panic, string concatenation, and calls to compiler-designated pure constant functions. Constant evaluation has step and memory limits and cannot perform I/O, allocate mutable containers, access environment state, spawn tasks, invoke FFI, or observe engine identity.

The same evaluator is used by the compiler and formatter tests; failure to evaluate a required top-level constant is a compile error with a bounded trace.

## 14. Deliberate omissions from 1.0

The following are not merely unimplemented; source compatibility must not assume them:

- `null`, `undefined`, optional object properties, and implicit nullable types;
- exceptions and `try/catch`;
- classes, inheritance, prototypes, and method bodies inside records;
- `impl`, nominal conformance declarations, associated types, and trait specialization;
- source-level `async`, `await`, futures, generators, and executor selection;
- user-defined macros, build scripts, operator overloads, or implicit conversions;
- shared-memory user threads and user-visible locks;
- reflection capable of mutating private fields or forging types;
- undefined behavior in safe source.

## 15. Generated source grammar

The complete candidate 0.1 grammar is the checked-in [generated Ruda source
grammar](../generated/source-grammar.md). Its machine-readable source is
`bootstrap/tables/grammar-productions.tsv`; generation validates names,
references, token terminals, EBNF delimiters, reachability from `file`, and
committed-output freshness. The production-indexed fixture manifest is
`tests/syntax/parser/production-coverage.tsv`.

Rows marked `baseline` restate the accepted design in this specification and
the accepted decisions. A `provisional` row identifies syntax that still needs
an accepted decision before compatibility is frozen. The generated grammar
becomes normative only when the I1 syntax-freeze gate is verified with every
production covered and no provisional row remaining. Until then, this section
does not turn parser over-acceptance into supported source syntax.

## 16. Conformance rule

An implementation conforms to this baseline only when every accepted construct has equivalent VM/native behavior, every rejection has a stable diagnostic class, the canonical formatter can round-trip it, and the public conformance suite passes. Parsing a construct without implementing all four surfaces does not constitute language support.
