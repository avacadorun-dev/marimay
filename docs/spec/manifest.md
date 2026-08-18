# Ruda manifest syntax — design baseline 0.1

Status: normative language-toolchain contract.

This document defines the bounded syntactic model and canonical bytes shared by I1 formatting and I7 package semantics under D027. Parsing and formatting a manifest does not claim that module names, versions, capabilities, dependencies, or workspace membership are semantically valid.

## 1. Input model

`ruda.mod` and `ruda.work` are UTF-8 text. They accept LF, CRLF, or lone CR input newlines and horizontal ASCII space or tab between tokens. Empty physical lines are allowed. A byte-order mark, NUL outside a quoted escape, unescaped control byte, or malformed UTF-8 is invalid. There is no manifest comment syntax in 0.1; `#` and `//` are ordinary invalid input rather than comments that might be silently discarded.

Quoted values use JSON string syntax: double quotes; escapes `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, and `\t`; and exactly four hexadecimal digits after `\u`. A high-surrogate escape must be immediately followed by a low-surrogate escape, and an isolated low surrogate is invalid. Unescaped content is valid UTF-8 and cannot contain a control scalar.

The line-oriented grammar is:

```text
mod-file       = { blank-line | mod-directive } EOF ;
mod-directive  = ("module" | "language") string line-end
               | ("require" | "capability") "(" line-end
                   { blank-line | string string line-end }
                 ")" line-end
               | metadata-name string line-end ;
metadata-name  = name-part "." name-part ;
name-part      = ASCII-lower { ASCII-lower | ASCII-digit | "_" | "-" } ;

work-file      = { blank-line | "use" string line-end } EOF ;
blank-line     = horizontal-space line-end ;
line-end       = horizontal-space (newline | EOF) ;
```

Known directive names cannot be reused as metadata namespace parts without a dot. A metadata directive has exactly one string value. Under D048, commands that construct a local module graph require exactly one `module` directive and exactly one `language` directive, with nonempty NUL-free decoded values; violations use `K0005`. Duplicate dependency/capability entries, version-constraint meaning, capability meaning, and metadata recognition remain I7 semantic checks. The I1 parser still retains syntactically valid repetitions so the formatter and later semantic layers consume one lossless tree.

At most one `require` group and at most one `capability` group may appear in a `ruda.mod` file. Repeating either group is a syntactic error because canonical formatting cannot preserve two distinct group boundaries. Entries inside the single group may repeat; duplicate-entry meaning remains an I7 semantic check.

Parser work, entries, diagnostics, decoded-string bytes, and nesting are explicitly bounded. A syntax diagnostic leaves the file unmodified.

## 2. Canonical `ruda.mod`

Canonical output uses LF and exactly one final newline. JSON strings are decoded and re-emitted with literal valid non-ASCII UTF-8, short escapes for quote, backslash, backspace, form feed, newline, carriage return, and tab, and lowercase `\u00xx` for other control bytes. `/` is not escaped.

Nonempty sections appear in this order, separated by one empty line:

1. every `module` directive, in source order;
2. every `language` directive, in source order;
3. one `require (` group containing every requirement;
4. one `capability (` group containing every capability;
5. metadata directives.

Requirement and capability entries are sorted by decoded first value, then decoded second value, using unsigned UTF-8 byte order. Metadata is sorted by ASCII directive name, then decoded value. Equal entries remain equal regardless of host sort stability. Canonical groups use two-space indentation:

```text
require (
  "ruda-lang.example/postgres" "^1.4.0"
)
```

An explicitly present empty group remains present. Directives use one ASCII space between tokens and have no trailing whitespace.

## 3. Canonical `ruda.work`

Every directive is `use "relative/path"` on one line, with no blank lines between directives and one final newline. The decoded path is normalized with portable `/` separators and lexical `.`/`..` removal. It must remain relative, nonempty, NUL-free, and cannot escape the workspace root. Entries are sorted by normalized decoded path in unsigned UTF-8 byte order. Duplicate paths remain a later semantic diagnostic rather than being silently removed by formatting.

## 4. Diagnostics and verification

Malformed directive/group syntax uses `K0001`, malformed strings use `K0002`, and an invalid/non-contained workspace path uses `K0003`. Diagnostics carry the manifest snapshot and exact physical byte span. Defensive formatter limits use `F0003` in the public formatting workflow.

Golden fixtures cover every directive, empty/nonempty groups, ordering, JSON escape equivalence and surrogate boundaries, newline variants, workspace normalization, invalid recovery, limits, allocation failure, parse/format/parse equivalence, and idempotence.
