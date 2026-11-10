---
name: Decision
about: A choice that must be recorded before work can start
title: "decision: "
labels: "decision-needed"
assignees: ""
---

Use this when the blocker is a choice, not an implementation. Open gates that
have waited for months are usually decisions nobody took.

**Question**
One sentence. If it needs "and", it is two decisions.

**Options**
Two or three, each with the consequence that makes it cost something:
what it buys, what it costs, and what it forecloses.

**Recommendation**
Which one you would take today, and why.

**Done when**
A decision record in `docs/design/decisions.md`, plus every affected projection:
`roadmap.tsv`, the product roadmap row, and the spec or release contract text
that quoted the old answer.

**Timebox**
A decision is one week at the stated capacity, not a research project. If the
evidence is missing after that, record the decision under uncertainty and name
what would reverse it.
