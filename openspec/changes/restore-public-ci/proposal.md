# Proposal

## Why

The first public CI run failed before several platform and sanitizer suites
could exercise Marimay's existing contracts. This change records the repairs,
integrates the concurrent organization/OpenSpec setup, and makes their final
verification traceable.

## What Changes

- Repair C17 alignment, initialization, MSVC native compilation, exact host
  arguments/streams, runtime artifacts, and portable test fixtures.
- Preserve parser resource-limit classification and compare formatter output
  according to the accepted separator rules; reconcile D071 wording and grammar.
- Keep JIT sanitizer entry probes inside owned readable code memory while
  preserving publication protection, budgets, and failure cleanup.
- Correct CI setup, history scanning, diagnostics, and the aggregate IPO test
  timeout while retaining strict warnings, assertions, and full failure sweeps.
- Connect OpenSpec planning records to the existing specification, decisions,
  focused owners, roadmap, security policy, and platform evidence.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None. These repairs restore existing requirements, so this change declares
`skip_specs: true`. D071's grammar and formatter clarification is recorded in
the existing normative documents and accepted decision.

## Impact

Relevant [requirements](../../../docs/requirements.md) include IMPL-001,
IMPL-004, EXEC-001, EXEC-004, EXEC-021, EXEC-023, TOOL-003, TOOL-025, and TOOL-026.
The [accepted decisions](../../../docs/design/decisions.md), especially D071,
D075, and D111, and [normative specifications](../../../docs/spec/) govern the
behavior. Compiler base/parser/VM/native owners, Windows platform adapters,
focused tests, CMake/CI configuration, and contributor documentation are affected.
The scope is the existing preview implementation and its verification process.
