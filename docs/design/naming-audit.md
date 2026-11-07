# N0 naming and distribution identity audit

Status: **open release blocker for external distribution identity**. D001 now fixes the official identity as `Marimay`, executable `mmay`, source suffix `.my`, bundle suffix `.myb`, and `mmay.*` project/tool-state names. This task does not select, register, reserve, control, or legally clear domains, accounts, trademarks, organization/package namespaces, or the placeholder example namespace `marimay.example`.

## Historical candidates and confirmed collision

As of 2026-08-10, the Go package index publishes [`github.com/whoppingdisc/malang`](https://pkg.go.dev/github.com/whoppingdisc/malang) as a command module. Its indexed README is titled “Malang Compiler”, calls its Malang project a programming language, and instructs users to build `-o malang`. The indexed pseudo-version was published 2025-05-16, is MIT licensed, and reports `Imported by: 0`.

This is direct historical evidence that the repository's former working name **Malang** collided with a public language/project name. Low indexed adoption reduced practical migration pressure but did not remove search, package, social-identity, or legal ambiguity; the historical D001 therefore replaced that working name with **Ruda**. The other project's documented executable is `malang`; it does not establish a collision with the new `mmay` command.

The historical search also surfaced other projects using “Malang” for programming-language work. Ruda was a later pre-release candidate, not a cleared or published identity, and the repository owner has now superseded it with Marimay. N0 preserves both former names and the dated collision evidence as history; none of that evidence clears Marimay.

## Required evidence matrix

N0 owns one versioned report covering:

1. **Language and product name:** `Marimay` and confusingly similar software/language names across source hosts, language lists, package indexes, app stores, documentation/search results, and developer communities.
2. **Legal identity:** trademark/company/product searches in the initial distribution jurisdictions, relevant Nice classes, counsel or qualified-review scope, unresolved marks, and a dated risk conclusion. Repository research is not legal clearance by itself.
3. **Domains and public handles:** exact/defensible domains, Git hosting organization, registry host, documentation host, security-contact address, and primary social/community identifiers. Availability checks are dated; verification requires reservation/control evidence, not a search result.
4. **Package namespace:** unique organization, canonical module-path prefix, registry namespace, standard-library namespace, signing identity, and typo/confusable analysis. Placeholder `marimay.example` cannot become a published contract without control evidence.
5. **Command and PATH identity:** `mmay` checked independently on every Tier 1 OS against default commands and supported installers/package managers (including their case-folding, aliases, shims, and uninstall behavior). A clean-image install/upgrade/uninstall matrix proves there is no destructive PATH takeover; migration evidence also proves the former `ruda` and `ma` executables are not installed as hidden aliases.
6. **Artifact identity:** `.my`, `.myb`, `mmay.mod`/`mmay.work`/`mmay.lock`, MIME/editor associations, service names, environment-variable prefix, cache/config directories, and diagnostic/protocol schema prefixes.
7. **Migration completion:** inventory of code, specs, examples, generated registries, package paths, caches, protocol strings, signing keys, websites, CI, installers, and compatibility aliases affected by the Ruda-to-Marimay rebrand.

## Gate and deadline

The repository-side naming migration may complete while N0 remains `in-progress`. N0 is `verified` only when every external evidence row links direct dated evidence, the Marimay migration audit is clean, and a follow-up decision records the controlled public organization/registry/module identity and residual legal/search risk.

N0 blocks:

- I7 registry/package namespace implementation beyond replaceable internal prototypes;
- public/indexed E0 previews (private invitation-only usability work may use an explicit working codename);
- P0 alpha publication, package publication, domain announcement, and signing-identity creation;
- any claim that `Marimay`, `mmay`, or `marimay.example` is a legally cleared, available, reserved, or controlled external release identity.

Popularity, `Imported by` count, an unused exact domain, or absence from one trademark database cannot independently close the gate.

## Public source repository scope (D112)

The owner authorizes the public source repository as a personal learning and
hobby project. This is not a public toolchain alpha, supported installer,
registry publication, or legal clearance. The N0 release restrictions and
required evidence remain open. The source import follows the
[privacy publication policy](../security/publication.md).
