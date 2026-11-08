# Design

## Context

See `proposal.md` and the capability delta. D030/D064/D083 currently pin a
build-time compiler and reserve public CLI-alpha for wider gates. D114 explicitly
permits this owner-authorized experimental Mac preview and its scoped compiler
policy; it does not close CLI0/CP40. D113/D115 belong to concurrent local LSP work
and are not included. Work uses a clean checkout and leaves those edits intact.

## Goals / Non-Goals

**Goals:** independently usable ARM64/Intel macOS 15+ downloads; system-owned
Apple CLT without CI paths; collision-aware global installation; verified,
repeatable tag publication; source/privacy preservation.

**Non-Goals:** signing/notarization, production support, arbitrary C compilers,
cross compilation, public bytecode, LSP integration, package registries or
promotion of existing product milestones. No source syntax changes.

## Decisions

1. Keep archive producer identity exact. Native and doctor owners select fixed
   `/usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin LC_ALL=C /usr/bin/clang`
   only in the explicit portable installed mode. Probe and compile specify
   architecture and deployment target. AppleClang >=16, exact family/target and
   bounded numeric version parsing are required. Cache/doctor use actual local
   version/build/target plus policy, not producer constants. Build-tree/default
   Windows/Linux remain pinned. Clearing child environment preserves system
   `xcode-select` and avoids a broader process-environment API or trusted `CC`.
2. Use macOS `pkgbuild` with a versioned complete prefix under
   `/usr/local/lib/marimay/V`. Package scripts validate ownership and payload;
   payload staging never directly overwrites the active global command. Publish
   an absolute-path POSIX wrapper at `/usr/local/bin/mmay` last, via atomic
   rename. This avoids symlink-dependent executable lookup. Standard Installer
   handles payload/receipt lifecycle; we promise atomic activation, not atomic
   file-by-file macOS payload installation. Any existing candidate version
   prefix, including a same-version reinstall, is rejected unchanged. Upgrades
   require a fresh version slot. Existing Installer receipts must match the
   verified old active wrapper/prefix and exact package/version/location/volume;
   stock `pkgutil` represents receipt locations relative to the target volume,
   so compare exactly `usr/local/lib/marimay/V` without a leading slash.
   partial, corrupt or foreign records fail closed without replacement.
   The CI upgrade fixture registers a real old receipt/BOM through Installer,
   then adds unrelated data outside its BOM and verifies its preservation.
   A failed new payload stays inactive and may
   require explicit recovery; it is not silently removed or overwritten.
   Signed `.pkg` would require unavailable Apple identity; a shell/Python
   installer would add prerequisites and a separate archive-extraction surface.
3. Add tag-scoped release workflow and manual dispatch. Read-only jobs validate
   metadata/history, reuse source-security checks, build/test/package/install on
   both Mac hosts and verify a successful complete C17 workflow for the exact
   release commit. The publishing job alone receives `contents:write`, validates
   downloaded assets and tag identity again, uploads to a draft, then publishes
   as a prerelease. Existing public assets are immutable to reruns. Inputs go
   through validated environment/structured arguments, not shell interpolation.
4. Repair the single bad merge with an independently verified private bundle,
   identical tree/message/parents and noreply identity. Coordinate before a
   `force-with-lease` restricted to the expected old main. Base new work on the
   repaired commit; never revive the retired private ancestry or payloads.
5. Use channel `experimental-alpha`, bounded claim and readiness false; update
   metadata, normative text and tests together. Preserve every roadmap state.
   OpenSpec links TOOL-018/019/021/023/025/026 and EXEC-002/TOOL-006; authority
   remains the normative repository documents and accepted D114.

## Ownership and Verification

Native toolchain/build/doctor owners implement selection, parsing, cache and
allocation failure tests. Distribution tooling owns `.pkg` generation/scripts
and collision/activation tests. Release metadata/docs owner updates contracts;
workflow coordinator owns validation and publication, not compiler semantics.

All new allocation-capable C paths require all reachable deterministic failure
indices and zero live allocation/byte counters. Exercise missing/nonzero/signal/
limited compiler probes, malformed/foreign/old/wrong targets and poisoned env.
Retain exact runtime corruption tests. Run full CTest on both Mac packages,
then install and invoke from outside source/build trees: doctor, VM run/test,
native debug/release outputs and differential checks. Check package inventory,
pre/postinstall conflicts, interrupted/failed activation, upgrade/reinstall and
wrapper paths. Preserve existing Tier 1 CI/sanitizer/fuzz coverage. Run all
architecture, roadmap, generated, spec/example/docs, memory, release, publication
and OpenSpec gates. Evidence is exact-revision/platform scoped.

## Risks / Trade-offs

- Unsigned package and CLT prerequisites -> explicit release/install notes,
  checksums and real Installer smoke tests; no Gatekeeper disabling.
- Platform CLT changes -> bounded compatibility policy and actual cache identity;
  no promise that compiler version alone proves all SDK ABI compatibility.
- Package interruption -> old active command preserved; inactive payload may
  remain under Installer management. Do not claim full package atomicity.
- Concurrent contributors -> separate checkout, reserved decision IDs, ref lease
  and coordinated main transition; leave dirty LSP changes untouched.

## Migration Plan

Prepare and verify metadata-only repair, publish the guarded replacement, then
publish/review the release implementation. After exact-head CI passes, integrate
with noreply identity, push the first alpha tag and follow release workflow to
published assets. Download and verify the actual published package in a temporary
prefix/staging environment; deliver the release URL and global install command.
Do not roll back public assets by overwriting them; fix a failed unpublished
draft or use a new alpha sequence for a published correction.
