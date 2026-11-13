# 0.2.0-alpha.4 prepared package, validated locally

Local macOS arm64 evidence, 2026-11-13, at compiler revision
`9cc2a41edbb8c528c9f5c15bda8b46d93a111df2` (version `0.2.0-alpha.4`, channel
`experimental-alpha`, claim `local-checker-backed-slice`,
`publicationReady: false`). This record covers issue #28's "a checksummed macOS
package is built and validated locally" item only. **No tag exists, nothing was
uploaded, and no release was created**: publication needs the owner's explicit
authorization and an exact-tag workflow pass (D114, D128).

## Environment

| Field | Value |
|---|---|
| Host | macOS 27.0.1 arm64, Apple clang from `/usr/bin/clang` |
| Configuration | `Release`, `MMAY_PORTABLE_MACOS_TOOLCHAIN=ON`, `MMAY_WARNINGS_AS_ERRORS=ON`, deployment target 15.0, `CMAKE_OSX_ARCHITECTURES=arm64` |
| Build tree | `build/alpha` |
| Source revision | `9cc2a41` (tree clean at build time apart from the version bump under test) |

## Produced assets

| Asset | Bytes | SHA-256 |
|---|---:|---|
| `marimay-0.2.0-alpha.4-darwin-arm64.pkg` | 10640157 | `5baabd4099dc5fe31589d5e6061c8bd6a866b738b86ab2c0a193c560614f150d` |
| `marimay-0.2.0-alpha.4-darwin-arm64.tar.gz` | 10114297 | `ef9006e68f6515fb71f513493826541d4fbec32d0787152366c37328f5593ed0` |
| `marimay-0.2.0-alpha.4-source.tar.gz` | 9759301 | `5962acca8d599790116730d8d2876afd81a69953478465e2124262df212cbe45` |

Each asset has a `*.sha256` sibling written by the packaging step; all three
siblings were re-verified against the files (`tools/macos_payload.verify_checksum`)
before the checks below.

## What was verified

- **Installed prefix inventory and hashes** (`mmay_payload.verify_prefix`): the
  staging prefix carries exactly one version manifest,
  `share/mmay/0.2.0-alpha.4/checksums.sha256`, every listed file matches its
  digest, the tree contains no links or special files, and the inventory has no
  unlisted entries.
- **Archive relocation** (`unpack_archive` + `verify_prefix` with
  `expected_revision`): the host `.tar.gz` was unpacked into a path containing a
  space, and the 1371-file prefix verified again, including the provenance
  schema, the release metadata and the source revision it names.
- **Archive inventory** (`tools/release/assets.inspect_tar`): member names are
  normalized and unique, no member is absolute or escapes the root, both native
  runtime profiles and the required headers are present, the installed checksum
  manifest matches the archive contents, and the metadata reports
  `compiler/runtime/standardLibrary` `0.2.0-alpha.4`, channel
  `experimental-alpha`, claim `local-checker-backed-slice`,
  `publicationReady: false`. The source archive was checked the same way.
- **The relocated prefix actually works**: from the unpacked prefix, `mmay
  version --json` reports the new version and the honest claim; `fmt --check`,
  `check`, `test --engine both`, `run`, and a native `build` of
  `tests/public-alpha/cp29-quick-start` all succeed, and the built binary prints
  `Marimay CP29 quick start`. No system installation was used for this step.
- **Package layout** (`pkgutil --expand-full`): the package is a flat component
  package with `Bom`, `PackageInfo`, `Payload` and `Scripts`; the payload
  verifies as a prefix and its inventory is byte-for-byte the same file set as
  the archive and the staging prefix (1372 files including the manifest). The
  `PackageInfo` declares identifier `org.marimay.mmay.0.2.0-alpha.4`, version
  `0.2.0-alpha.4`, `install-location="/usr/local/lib/marimay/0.2.0-alpha.4"`,
  `minimumSystemVersion="15.0"`, and the three installer scripts
  (`installer-common.sh`, `preinstall`, `postinstall`) are present in the
  expanded `Scripts` directory.
- **Signature state** (`pkgutil --check-signature`): `Status: no signature`,
  exit 1 — the expected unsigned/unnotarized state documented by the release
  contract, not a defect of this build.

## What was not verified, and why

- **The real Installer path** (`tools/test-macos-install` with
  `--allow-system-install`) was not run: it writes to `/usr/local`, creates
  receipts, and refuses to start when a global `mmay` or a previous prefix
  exists, which is the ephemeral-runner contract it enforces. That gate runs on
  the release workflow's macOS runners for the exact tagged revision and is a
  publication precondition, not an item this local pass can claim.
- **x86_64** was not built here; the workflow builds both architectures on
  separate runners.
- **Nothing was published, tagged, signed, or notarized.**

## Reproduce

```sh
cmake -S . -B build/alpha -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DMMAY_WARNINGS_AS_ERRORS=ON -DMMAY_PORTABLE_MACOS_TOOLCHAIN=ON \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/alpha --parallel 6
cmake --build build/alpha --target mmay_package
python3 tools/package-macos --install-root build/alpha/distribution/stage \
  --output-dir build/alpha/distribution --version 0.2.0-alpha.4 --arch arm64
```

The validation script used for the checks above lives outside the repository (a
scratch file under `build/alpha/`); every check it performs is one of the
repository's own functions named above.

## Consequence

The release contract, changelog and limitations are consistent with the built
artifact, and the artifact is internally consistent. This is evidence that the
package can be produced and validated; it is not evidence that the tagged
revision passes the release workflow, and it authorizes no publication.
