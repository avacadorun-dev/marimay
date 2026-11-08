# Install the experimental macOS alpha

`0.1.0-alpha.1` is an experimental learning-project preview for macOS 15+ on
Apple Silicon and Intel. Its channel is `experimental-alpha`, its claim is
`local-checker-backed-slice`, and `publicationReady: false` records the still
open CLI0/CP40 audit. See the [release contract](releases/0.1.0-alpha.1.md) and
[limitations](known-limitations.md). Release availability and exact artifact
hashes are recorded on [GitHub Releases](https://github.com/marimay-lang/marimay/releases).

## Prerequisites

Choose `arm64` for Apple Silicon or `x86_64` for Intel; `uname -m` reports the
current shell architecture. Use the package matching the physical Mac, without
translation. Native builds require configured Apple Command Line Tools with
AppleClang 16 or newer. If they are missing, run Apple's installation action:

```sh
xcode-select --install
```

The installed portable policy uses the system Apple compiler in a cleared
environment with an explicit architecture and macOS 15.0 deployment target.
`CC`, user PATH, SDK overrides and project settings cannot replace it.
`mmay doctor` reports the actual selected identity and both runtime profiles.
Formatting, checking and VM execution do not invoke the host C compiler.
Python, CMake and a source checkout are not installation/runtime prerequisites.

The first package is unsigned and unnotarized. Checksums, provenance and SBOM
do not establish Developer ID signing or Apple notarization. Review the named
release and artifact before approving installation; do not disable macOS
security controls.

## Install the global command

Download the matching `.pkg` and `.pkg.sha256` from the same release into one
directory. For Apple Silicon:

```sh
shasum -a 256 -c marimay-0.1.0-alpha.1-darwin-arm64.pkg.sha256
sudo /usr/sbin/installer -pkg ./marimay-0.1.0-alpha.1-darwin-arm64.pkg -target /
/usr/local/bin/mmay version --json
/usr/local/bin/mmay doctor --json
```

On Intel, use `darwin-x86_64` in both filenames. You can also open the verified
package in macOS Installer and approve its administrator prompt.

The package installs the complete tree under
`/usr/local/lib/marimay/0.1.0-alpha.1` and activates an owned POSIX wrapper at
`/usr/local/bin/mmay`. The wrapper executes the real binary in that versioned
tree; it is not a symlink. It is activated last by atomic rename after payload
validation. The installer edits neither PATH nor shell profiles. When
`/usr/local/bin` is already on PATH, `mmay` works from any directory; otherwise
use its absolute path or add that directory to your shell PATH explicitly.

Try a new project outside the downloaded files:

```sh
mmay init hello
mmay fmt --check hello
mmay check hello
mmay run hello
mmay test hello --engine both
mmay build hello --release --output hello-app
./hello-app
```

The complete prefix includes the VM, both native runtime profiles, headers,
bounded standard-library source, offline docs, source kit, provenance and SBOM.
Moving only `bin/mmay` loses required distribution files.

## Archive installation

The matching `.tar.gz` contains the same complete prefix, with `bin`, `include`,
`lib` and `share` at its root. Verify its `.tar.gz.sha256`, then extract into a
new empty directory you own. For Apple Silicon, using a previously missing
destination:

```sh
shasum -a 256 -c marimay-0.1.0-alpha.1-darwin-arm64.tar.gz.sha256
mkdir -p "$HOME/.local/opt/marimay"
mkdir "$HOME/.local/opt/marimay/0.1.0-alpha.1"
tar -xzf marimay-0.1.0-alpha.1-darwin-arm64.tar.gz \
  -C "$HOME/.local/opt/marimay/0.1.0-alpha.1"
"$HOME/.local/opt/marimay/0.1.0-alpha.1/bin/mmay" doctor --json
export PATH="$HOME/.local/opt/marimay/0.1.0-alpha.1/bin:$PATH"
```

Use the Intel filenames on Intel. The final `export` changes the current shell
only. Complete-prefix relocation is supported; it does not install a global
wrapper or edit a shell profile.

## Upgrade, uninstall and interrupted installation

The package rejects every existing candidate version prefix, including a
same-version reinstall or a partial prior attempt, without modifying it. This
is deliberate: overwriting an active version file by file cannot preserve the
old working command on failure. A later alpha uses a new version slot and
activates its wrapper only after validation. Foreign or modified command
collisions also fail rather than overwrite the command.

The installer also refuses foreign, partial, corrupt or unreadable Installer
receipts for its identifier. An existing receipt must match the verified old
active version, location and target volume; it is not replaced merely because
the new version directory is missing. Receipt checks do not list unrelated
installed software. Do not forget a receipt to bypass this ownership check.

Atomic activation does not make the entire macOS payload transaction atomic.
An interrupted install can leave an inactive version directory or receipt.
Inspect the failed slot and Installer log before explicit recovery; retrying
the same package does not erase or repair that slot automatically.

The package receipt identifier is `org.marimay.mmay`. `pkgutil --pkg-info` and
`pkgutil --files` with that identifier inspect the installed receipt. For an
intact prefix, run `shasum -a 256 -c share/mmay/0.1.0-alpha.1/checksums.sha256`
from its root to check owned files. Before uninstall or recovery, verify the
wrapper still has the managed-launcher marker and targets the version being
removed, and preserve every unrelated or modified file. Remove an owned global
wrapper only as an explicit administrator action after that inspection.

The bundled `MarimayDistributionLifecycle.cmake` supports checksum-validated
prefix uninstall when CMake 3.25+ is available; CMake is needed only for this
optional recovery path. It refuses modified owned files and preserves unrelated
content. macOS Installer itself provides no automatic uninstall command.
`pkgutil --forget org.marimay.mmay` removes only the receipt, not installed files;
use it only after deliberate file cleanup. Never delete the whole version tree
to bypass a failed ownership or checksum check.
