# Experimental editor support

Build the current compiler with `cmake --preset release` and
`cmake --build --preset release --parallel`. `build/release/mmay lsp` starts the
built-in language server over standard input/output. It is intended to be
launched by an editor, so it waits for framed protocol messages.

The basic server provides compiler errors/warnings for unsaved text, navigation
to function/type/constant, local/parameter and record-field definitions,
references, checked type hover, completion, validated rename, document symbols,
package-prefix symbol search, and built-in formatting.
It uses the same compiler service as `mmay check`; source files are not changed
by diagnostics or navigation. Formatting returns an edit for the editor to apply.

## Zed

Install the companion [Marimay Zed extension](https://github.com/marimay-lang/marimay-zed)
as a development extension using **zed: install dev extension** and select its
checkout. The extension supplies Tree-sitter highlighting and launches `mmay lsp`.
Place the newly built compiler on the project PATH, or set its absolute path in
Zed user settings (replace the example path):

```json
{
  "lsp": {
    "marimay": {
      "binary": {
        "path": "/absolute/path/to/marimay/build/release/mmay",
        "arguments": ["lsp"]
      }
    }
  }
}
```

Open a `.my` file. The language indicator should read **Marimay**. Compiler
diagnostics appear after opening or editing; use Zed's **Go to Definition**,
hover, references, symbol search, completion, **Rename**, and **Format Buffer** actions.
On macOS, Command-click opens a definition. The Rename shortcut follows the chosen
Zed keymap (for example, Shift-F6 in the JetBrains keymap). If startup
fails, **zed: open log** shows the server launch error. A compiler from before
D113 does not recognize `lsp`; rebuild it and restart the language server.

## Current limits

Checking is synchronous. Up to four current package analyses are retained, so
hover, definitions, references and diagnostics reuse checked facts while text is
unchanged. Clean-buffer completion uses the same facts; an unfinished expression
may still require a private repair check. Source/manifest bytes are validated and
accepted editor changes, saves and file-watch events invalidate the cache. Clients
without dynamic file watchers can refresh newly added/deleted closed files by
saving a document or restarting the server. Local modules and standalone scripts
are supported; remote modules
and background workspace indexing are not. Full-document synchronization and
UTF-16 positions are advertised. At most 64 documents and 64 MiB of open text
are retained; individual source/protocol bodies are bounded to 16 MiB.

Navigation uses published compiler facts, including scoped locals/parameters
and nominal record fields. Completion supports ordinary names and simple dotted
receivers such as `box.` or `box.va`; it can privately repair an unfinished
expression to obtain checked candidates. General call/index receivers, generic
field-chain substitution, arbitrary malformed-buffer completion, generic local specializations and enum payload
identities remain future work. Syntax/type errors can suppress navigation and
rename until the module checks cleanly.

Definitions in standard packages open exact compiler source views under the
project's `.mmay/lsp/stdlib/` directory. Keep a project source file open and
checking successfully to navigate within these views. These are for reading; editing a view does
not change the standard library or add source files to the project. JSON
convenience calls such as `json.encode` and `json.decode` are compiler intrinsics:
their definition view explicitly shows the registry API signatures, rather than
claiming a source implementation. Stream operations such as `io.println` navigate
to their real external declarations.

Errors in another file that block checking include a related location to open.
Invalid `mmay.mod` files show the real compiler message and location. Semantic
navigation can still be unavailable while syntax/type errors suppress checked
facts; the server does not guess definitions in that state.

Rename proposes exact edits after checking the complete local module and
verifying binding identities, including unchanged references. It refuses
capture, collisions, ambiguous shorthand positions and unindexed occurrences.
When initiated from an unambiguous declaration, record shorthand may expand to
`field: value` to preserve both meanings. Zed applies the returned edits.
Semantic tokens and asynchronous cancellation remain future work.
Tree-sitter highlighting works independently of the server.
This local editor slice does not mark I8 or all-platform acceptance complete.

## Verification

```sh
ctest --test-dir build/debug -R 'mmay_lsp' --output-on-failure
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-roadmap
```

The protocol tests cover live-pipe framing, unsaved errors and repairs,
cross-file/package and standard-library navigation, cache invalidation, dependency/manifest
error locations, references, formatting, stale versions, Unicode file
URIs and UTF-16 positions, completion, shadowing, field ownership, safe rename
and capture rejection. C owner tests enumerate deterministic allocation
failures and require both zero live allocations and zero live bytes after
teardown. Sanitizer and supported-platform release gates remain separate.

The [editor behavioral fixture](../tests/lsp/fixtures/editor_features_test.my)
combines a mutable record field, constructor shorthand, a shadowed local and
a captured variable. Its VM/native test remains equal while editor facts are
published. For example, complete `box.va`, use F12 on `value`, or rename the
outer local from its declaration; the nested `value` remains a separate binding.
