# Public source publication

Marimay is a personal programming language built for learning and fun. Its public source repository
is a place to explore implementation and write small programs, not a production
release announcement or a claim to replace another language.

D112 records the owner-authorized public source import under MIT. N0 and all
product/platform release gates stay open. Public source availability does not
clear trademarks, registries, domains, signing identities, PATH coexistence,
alpha readiness, performance parity, or the target language feature set.

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
