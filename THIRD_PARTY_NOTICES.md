# Third-party notices

The [MIT License](LICENSE) applies to original Marimay code and documentation.
It does not replace the licenses or attribution of third-party material.

## Unicode data

The pinned Unicode 17.0.0 input files in `bootstrap/tables/unicode/` and derived
identifier/formatter tables use Unicode data. The full
[Unicode License V3](licenses/Unicode-3.0.txt) accompanies this repository and
installed documentation. Copyright © 1991–2026 Unicode, Inc. Input versions,
SHA-256 values, and upstream sources are recorded in
[the provenance note](bootstrap/tables/unicode/README.md).

## Optional SLJIT dependency

SLJIT is not vendored into the default bootstrap. An explicitly enabled JIT
build consumes the pinned upstream revision and file hashes in
[cmake/sljit-source.json](cmake/sljit-source.json). SLJIT is BSD-2-Clause
licensed; preparation verifies its license file, and enabled installations
include it as `licenses/SLJIT-BSD-2-Clause.txt` within the versioned documentation
root. Its upstream is [zherczeg/sljit](https://github.com/zherczeg/sljit).

## Logo

The original project logo was made by the repository owner with their daughter
and supplied for use here. The unmodified drawing is retained as a local asset;
the final banner places that drawing on a light background. No personal names or
photo metadata are included in these assets.
