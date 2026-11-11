# Public source publication

Marimay is a personal programming language built for learning and fun. Its public source repository
is a place to explore implementation and write small programs, not a production
release announcement or a claim to replace another language.

D112 records the owner-authorized public source import under MIT. N0 and all
product/platform release gates stay open. Public source availability does not
clear trademarks, registries, domains, signing identities, PATH coexistence,
alpha readiness, performance parity, or the target language feature set.

The owner separately authorized the first experimental macOS prerelease under
[D114](../design/decisions.md#d114-publish-a-bounded-experimental-macos-alpha).
Its `experimental-alpha` metadata keeps the `local-checker-backed-slice` claim
and `publicationReady: false`, which records incomplete CLI0/CP40 acceptance.
That limited authorization does not close N0 or permit a supported `cli-alpha`
claim, registry publication, signing, notarization or namespace reservation.
The [release contract](../releases/0.1.0-alpha.1.md) and
[CI gates](../ci.md#experimental-macos-release-flow) require exact-revision
security, build/test/install and immutable-asset validation before release.
Release checks preserve all source/history scanner rules. A failing metadata
check is repaired at its source rather than accepted through an allowlist. One
exception to that rule is recorded in
[D126](../design/decisions.md#d126-accept-six-historical-squash-merge-author-lines-as-reviewed-history)
and described [below](#reviewed-exception-in-commit-metadata); it exists because
the source of that finding cannot be repaired from inside the repository.

## History and privacy

The initial public import preserves the committed `main` lineage, parent
relationships, commit order, dates, and development changes. It substitutes
private author/committer contact data with the owner's public GitHub no-reply
identity and removes personal paths, private infrastructure endpoints, and
private repository references from historical file contents and messages.
Commit IDs necessarily change. Local branches, stashes, hidden tool refs,
reflogs, Git configuration, ignored files, and unfinished working-tree changes
are not part of the publication. The original local repository is retained.

Historical evidence is a privacy-edited copy, not byte-identical raw evidence.
Paths and endpoints are placeholders. Process inventories are omitted where
present. Existing recorded digests describe the historical experiment inputs;
this import is not a rerun or fresh milestone verification. Consult the
[review](review-2026-11-08.md) for the checks performed.

## Reviewed exception in commit metadata

Six squash-merge commits authored on 2026-11-11 carry the owner's GMail address
in the author line: `c8d92cc68d64124f27f4b0f8b3c155e13eeb37cb`,
`9139cca425a7233a0d890054199dc536a9e7e62d`,
`f2c9bf0c54398eeb3f3fccc7b2255af2943089d3`,
`53b216734c8f03118cadeea348b5c12c0048fc72`,
`a24e9e40365a092a035552a5d8a22e5d7e793d7a`,
`a9848597a580004a1cbde62c6c2fe46fb5f2ea5f`. The committer of all six is the
GitHub no-reply identity, which is what makes the source visible: GitHub wrote
the author line from the account's public commit address, so the repository-local
identity — already the no-reply address — could not prevent it and cannot repair
it. The address is the local part `avacado.run` at domain `gmail.com`, written
here in halves so that this page does not itself trip the guard it describes.

D126 accepts them as reviewed history. The exception keys on those six object
ids, only for a `commit/tag metadata` object, and only for the email finding:
personal paths, private endpoints and credentials are still rejected in the same
objects, and a new commit carrying the same address still fails. The scan prints
the suppressed count on every run, so the exception cannot quietly widen.

The durable lever is the account's public commit address; until it changes, a new
squash merge produces a new occurrence rather than a fixed one. The alternative
that would actually remove the address — rewriting history and asking GitHub to
purge unreachable objects and pull-request refs — stays available and unfree.

## Before publishing further changes

```sh
./tools/check-publication
./tools/check-publication --history
gitleaks git --config .gitleaks.toml --log-opts="--all" --redact=100 --max-archive-depth=3
gitleaks dir --config .gitleaks.toml --redact=100 --max-archive-depth=3 .
```

Run directory scans on a clean checkout so ignored build outputs do not dilute
the result. Review findings locally; do not upload raw secret reports. The
privacy guard and Gitleaks complement each other and can miss unknown formats,
encoded/encrypted values, or data outside the reviewed scope. Never infer an
absolute guarantee from a passing scanner.

Use synthetic hostnames under `.example`, documentation IPv4 ranges, relative
paths, and no-reply commit metadata. Do not push an older unedited clone to the
public remote: that would restore the original private history.

For pull requests, source-security CI scans the exact submitted head and its
full ancestry alongside fetched public branches and tags. It does not add
GitHub's temporary test-merge ref to that history scan. Metadata checks apply
unchanged to submitted commits and real branch/tag history; there is no
exception for a personal email in those objects. The build workflow separately
checks the PR merge result. See [CI](../ci.md) for checkout and permission scope.

## Private experimental implementation payloads

Experimental source snapshots, candidate patch bundles, compressed working
trees and archived experiment data are retained in the owner's private recovery
collection. They are excluded from the current public tree and all published
Git branches and tags. The exact originals and original Git histories remain
private; this publication cleanup does not integrate their experimental features.

The [artifact index](../implementation/evidence/private-artifacts.md) records
the 67 retired payloads and their private availability. Public reports retain
historical measurements and decisions, with links to provenance records instead
of implementation downloads. They do not claim public reconstruction of those
private experiments. Original source revisions and protocol input digests remain
historical identities; sanitized public commit identities can differ.

Required build inputs and conformance fixtures, including the independently
reviewed SLJIT compatibility patch under `cmake/`, remain part of the source.
