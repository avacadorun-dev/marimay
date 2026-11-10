#!/usr/bin/env python3
"""Reproducible structural checks for Marimay's D0 design baseline.

These checks intentionally use only the Python standard library.  They are
bootstrap-time documentation tools, not part of the eventual `mmay` toolchain.
The I1 parser and formatter replace the lightweight source scanner.
"""

from __future__ import annotations

import re
import sys
import unicodedata
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator, List, Optional, Sequence, Set, Tuple
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[1]

EXPECTED_KEYWORDS = {
    "as",
    "break",
    "const",
    "continue",
    "defer",
    "else",
    "enum",
    "extern",
    "false",
    "fn",
    "for",
    "from",
    "if",
    "import",
    "in",
    "interface",
    "let",
    "match",
    "mutable",
    "public",
    "return",
    "scope",
    "spawn",
    "true",
    "type",
    "unsafe",
    "while",
}

EXPECTED_REMOVED_GRAMMAR_WORDS = {
    "case",
    "of",
    "yield",
}

EXPECTED_RESERVED = {
    "null",
    "undefined",
}

REQUIRED_DECISIONS = {
    "D001",
    "D002",
    "D003",
    "D003a",
    "D004",
    "D005",
    "D006",
    "D007",
    "D008",
    "D009",
    "D010",
    "D011",
    "D012",
    "D013",
    "D014",
    "D015",
    "D016",
    "D017",
    "D018",
    "D019",
    "D020",
    "D021",
    "D022",
    "D023",
    "D024",
    "D025",
    "D026",
    "D027",
    "D028",
    "D029",
    "D030",
    "D031",
    "D032",
    "D033",
    "D034",
    "D035",
    "D036",
    "D037",
    "D038",
    "D039",
    "D040",
    "D041",
    "D042",
    "D043",
    "D044",
    "D045",
    "D046",
    "D047",
    "D048",
    "D049",
    "D050",
    "D051",
    "D052",
    "D053",
    "D054",
    "D055",
    "D056",
    "D057",
    "D058",
    "D059",
    "D060",
    "D061",
    "D062",
    "D063",
    "D064",
    "D065",
    "D066",
    "D067",
    "D068",
    "D069",
    "D070",
    "D071",
    "D072",
    "D073",
    "D074",
    "D075",
    "D076",
    "D077",
    "D078",
    "D079",
    "D080",
    "D081",
    "D082",
    "D083",
    "D084",
    "D086",
    "D087",
    "D088",
    "D089",
    "D090",
    "D091",
    "D092",
    "D093",
    "D094",
    "D095",
    "D096",
    "D097",
    "D098",
    "D099",
    "D100",
    "D101",
    "D102",
    "D103",
    "D104",
    "D105",
    "D106",
    "D107",
    "D108",
    "D109",
    "D110",
    "D111",
    "D112",
    "D113",
    "D114",
    "D115",
    "D116",
    "D117",
    "D118",
    "D119",
    "D120",
    "D121",
    "D122",
}

VALID_DECISION_STATES = {"accepted", "provisional", "deferred", "rejected"}
REQUIRED_PROTOTYPE_DECISIONS = {"D007", "D010", "D012", "D013"}


@dataclass(frozen=True)
class Finding:
    path: Path
    line: int
    message: str

    def render(self) -> str:
        try:
            display = self.path.relative_to(ROOT)
        except ValueError:
            display = self.path
        return f"{display}:{self.line}: {self.message}"


@dataclass(frozen=True)
class Token:
    text: str
    line: int
    column: int


def _read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except UnicodeDecodeError as error:
        raise ValueError(f"not valid UTF-8: {error}") from error


def _line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def _markdown_files() -> List[Path]:
    # Check source documentation, not installed copies in ignored output roots.
    return sorted(
        path
        for path in ROOT.rglob("*.md")
        if path.relative_to(ROOT).parts[0] not in {"build", "out", "dist"}
        and not any(part.startswith(".") for part in path.relative_to(ROOT).parts)
    )


def _heading_slug(text: str) -> str:
    text = re.sub(r"<[^>]+>", "", text.strip()).lower()
    text = unicodedata.normalize("NFKC", text)
    text = re.sub(r"[^\w\- ]", "", text, flags=re.UNICODE)
    text = re.sub(r"[\s-]+", "-", text).strip("-")
    return text


def _heading_anchors(text: str) -> Set[str]:
    anchors: Set[str] = set()
    occurrences: dict[str, int] = {}
    in_fence: Optional[str] = None
    for line in text.splitlines():
        fence = re.match(r"^\s*(`{3,}|~{3,})", line)
        if fence:
            marker = fence.group(1)
            if in_fence is None:
                in_fence = marker[0]
            elif marker[0] == in_fence:
                in_fence = None
            continue
        if in_fence is not None:
            continue
        heading = re.match(r"^#{1,6}\s+(.+?)\s*#*\s*$", line)
        if not heading:
            continue
        base = _heading_slug(heading.group(1))
        if not base:
            continue
        count = occurrences.get(base, 0)
        occurrences[base] = count + 1
        anchors.add(base if count == 0 else f"{base}-{count}")
    return anchors


def _link_destination(raw: str) -> str:
    raw = raw.strip()
    if raw.startswith("<"):
        end = raw.find(">")
        return raw[1:end] if end >= 0 else raw[1:]
    # Markdown titles follow whitespace. Repository-local paths containing
    # spaces must use angle brackets, so the first field is the destination.
    return raw.split(maxsplit=1)[0] if raw else ""


def check_doc_links() -> List[Finding]:
    findings: List[Finding] = []
    anchor_cache: dict[Path, Set[str]] = {}
    link_pattern = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")

    for path in _markdown_files():
        try:
            text = _read(path)
        except ValueError as error:
            findings.append(Finding(path, 1, str(error)))
            continue

        fence_char: Optional[str] = None
        fence_length = 0
        fence_line = 0
        for number, line in enumerate(text.splitlines(), 1):
            match = re.match(r"^\s*(`{3,}|~{3,})", line)
            if not match:
                continue
            marker = match.group(1)
            if fence_char is None:
                fence_char = marker[0]
                fence_length = len(marker)
                fence_line = number
            elif marker[0] == fence_char and len(marker) >= fence_length:
                fence_char = None
                fence_length = 0
                fence_line = 0
        if fence_char is not None:
            findings.append(Finding(path, fence_line, "unclosed Markdown code fence"))

        for match in link_pattern.finditer(text):
            destination = _link_destination(match.group(1))
            if not destination or destination.startswith("#"):
                continue
            if re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", destination):
                continue

            local, separator, fragment = destination.partition("#")
            local = unquote(local)
            target = (path.parent / local).resolve()
            try:
                target.relative_to(ROOT)
            except ValueError:
                findings.append(
                    Finding(path, _line_number(text, match.start()), "local link escapes repository")
                )
                continue
            if not target.exists():
                findings.append(
                    Finding(
                        path,
                        _line_number(text, match.start()),
                        f"missing local link target: {destination}",
                    )
                )
                continue
            if separator and fragment and target.is_file() and target.suffix.lower() == ".md":
                anchors = anchor_cache.get(target)
                if anchors is None:
                    anchors = _heading_anchors(_read(target))
                    anchor_cache[target] = anchors
                decoded = unquote(fragment).lower()
                if decoded not in anchors:
                    findings.append(
                        Finding(
                            path,
                            _line_number(text, match.start()),
                            f"missing Markdown heading fragment: {destination}",
                        )
                    )

    return findings


class MarimayScanner:
    """A bounded lexical sanity checker, deliberately not a parser."""

    def __init__(self, path: Path, text: str, base_line: int = 1) -> None:
        self.path = path
        self.text = text
        self.index = 0
        self.line = base_line
        self.column = 1
        self.tokens: List[Token] = []
        self.findings: List[Finding] = []
        self.delimiters: List[Tuple[str, int, int]] = []

    def _advance(self, amount: int = 1) -> None:
        for _ in range(amount):
            if self.index >= len(self.text):
                return
            if self.text[self.index] == "\n":
                self.line += 1
                self.column = 1
            else:
                self.column += 1
            self.index += 1

    def _error(self, message: str, line: Optional[int] = None) -> None:
        self.findings.append(Finding(self.path, line or self.line, message))

    def _skip_line_comment(self) -> None:
        while self.index < len(self.text) and self.text[self.index] != "\n":
            self._advance()

    def _skip_block_comment(self) -> None:
        start = self.line
        depth = 0
        while self.index < len(self.text):
            if self.text.startswith("/*", self.index):
                depth += 1
                self._advance(2)
            elif self.text.startswith("*/", self.index):
                depth -= 1
                self._advance(2)
                if depth == 0:
                    return
            else:
                self._advance()
        self._error("unterminated block comment", start)

    def _skip_quoted(self, quote: str, raw: bool = False) -> None:
        start = self.line
        self._advance()
        while self.index < len(self.text):
            character = self.text[self.index]
            if character == quote:
                self._advance()
                return
            if character == "\n":
                self._error("newline in quoted literal", start)
                return
            if character == "\\" and not raw:
                self._advance(2)
            else:
                self._advance()
        self._error("unterminated quoted literal", start)

    def _skip_interpolated(self) -> None:
        start = self.line
        self._advance(2)  # $"
        while self.index < len(self.text):
            if self.text.startswith("{{", self.index) or self.text.startswith("}}", self.index):
                self._advance(2)
                continue
            character = self.text[self.index]
            if character == '"':
                self._advance()
                return
            if character == "\n":
                self._error("newline in interpolated string", start)
                return
            if character == "\\":
                self._advance(2)
                continue
            if character == "}":
                self._error("unmatched '}' in interpolated string")
                self._advance()
                continue
            if character != "{":
                self._advance()
                continue

            expression_line = self.line
            self._advance()
            expression_start = self.index
            depth = 1
            quote: Optional[str] = None
            escaped = False
            while self.index < len(self.text) and depth > 0:
                current = self.text[self.index]
                if quote is not None:
                    if escaped:
                        escaped = False
                    elif current == "\\":
                        escaped = True
                    elif current == quote:
                        quote = None
                    self._advance()
                    continue
                if current in {'"', "'"}:
                    quote = current
                    self._advance()
                elif current == "{":
                    depth += 1
                    self._advance()
                elif current == "}":
                    depth -= 1
                    if depth == 0:
                        expression = self.text[expression_start : self.index]
                        nested = MarimayScanner(self.path, expression, expression_line)
                        nested.scan()
                        self.tokens.extend(nested.tokens)
                        self.findings.extend(nested.findings)
                    self._advance()
                else:
                    self._advance()
            if depth != 0:
                self._error("unterminated interpolation", expression_line)
                return
        self._error("unterminated interpolated string", start)

    def scan(self) -> Tuple[List[Token], List[Finding]]:
        matching = {")": "(", "]": "[", "}": "{"}
        while self.index < len(self.text):
            character = self.text[self.index]
            if character.isspace():
                self._advance()
            elif self.text.startswith("//", self.index):
                self._skip_line_comment()
            elif self.text.startswith("/*", self.index):
                self._skip_block_comment()
            elif self.text.startswith('$"', self.index):
                self._skip_interpolated()
            elif (
                character in {"r", "b"}
                and self.index + 1 < len(self.text)
                and self.text[self.index + 1] == '"'
            ):
                self._advance()
                self._skip_quoted('"', raw=character == "r")
            elif character == '"':
                self._skip_quoted('"')
            elif character == "'":
                self._skip_quoted("'")
            elif character in "([{":
                self.delimiters.append((character, self.line, self.column))
                self._advance()
            elif character in ")]} ".rstrip():
                expected = matching[character]
                if not self.delimiters or self.delimiters[-1][0] != expected:
                    self._error(f"unmatched '{character}'")
                else:
                    self.delimiters.pop()
                self._advance()
            elif character == "_" or character.isalpha():
                start = self.index
                line = self.line
                column = self.column
                self._advance()
                while self.index < len(self.text):
                    current = self.text[self.index]
                    if current == "_" or current.isalnum():
                        self._advance()
                    else:
                        break
                self.tokens.append(Token(self.text[start : self.index], line, column))
            else:
                self._advance()

        for opener, line, _column in self.delimiters:
            self._error(f"unclosed '{opener}'", line)
        return self.tokens, self.findings


def _scan(path: Path, text: str) -> Tuple[List[Token], List[Finding]]:
    return MarimayScanner(path, text).scan()


def _fenced_blocks(path: Path, language: str) -> Iterator[Tuple[int, str]]:
    text = _read(path)
    lines = text.splitlines(keepends=True)
    start: Optional[int] = None
    marker: Optional[str] = None
    body: List[str] = []
    for number, line in enumerate(lines, 1):
        opening = re.match(r"^\s*(`{3,}|~{3,})([^\s`]*)\s*$", line.rstrip("\r\n"))
        if start is None:
            if opening and opening.group(2).lower() == language:
                start = number + 1
                marker = opening.group(1)[0]
                body = []
            continue
        closing = re.match(r"^\s*(`{3,}|~{3,})\s*$", line.rstrip("\r\n"))
        if closing and closing.group(1)[0] == marker:
            yield start, "".join(body)
            start = None
            marker = None
            body = []
        else:
            body.append(line)


def _extract_word_fence(text: str, heading: str) -> Set[str]:
    pattern = re.compile(
        rf"^###\s+{re.escape(heading)}\s*$.*?^```text\s*$\n(.*?)^```\s*$",
        re.MULTILINE | re.DOTALL,
    )
    match = pattern.search(text)
    if not match:
        return set()
    return set(match.group(1).split())


def _decision_states(text: str) -> Tuple[dict[str, str], List[Tuple[str, int]]]:
    states: dict[str, str] = {}
    duplicates: List[Tuple[str, int]] = []
    headings = list(re.finditer(r"^##\s+(D\d{3}[a-z]?)\s+—.*$", text, re.MULTILINE))
    for index, heading in enumerate(headings):
        decision = heading.group(1)
        end = headings[index + 1].start() if index + 1 < len(headings) else len(text)
        section = text[heading.end() : end]
        state = re.search(r"^Status:\s+([a-z-]+)", section, re.MULTILINE)
        if decision in states:
            duplicates.append((decision, _line_number(text, heading.start())))
        states[decision] = state.group(1) if state else ""
    return states, duplicates


def _roadmap_gates(text: str) -> Set[str]:
    headings = set(re.findall(r"^##\s+([A-Z]\d+)\s+—", text, re.MULTILINE))
    dashboard = set(
        re.findall(
            r"^\|\s*((?:A|D|E|I|N|P)\d+|CLI\d+)\s*\|",
            text,
            re.MULTILINE,
        )
    )
    return headings | dashboard


def _requirement_rows(text: str) -> List[Tuple[str, str, int]]:
    rows: List[Tuple[str, str, int]] = []
    for number, line in enumerate(text.splitlines(), 1):
        match = re.match(r"^\|\s*([A-Z]+-\d+)\s*\|.*?\|\s*([^|]+?)\s*\|\s*$", line)
        if match:
            rows.append((match.group(1), match.group(2).strip(), number))
    return rows


def check_spec_terms() -> List[Finding]:
    findings: List[Finding] = []
    language_path = ROOT / "docs/spec/language.md"
    decisions_path = ROOT / "docs/design/decisions.md"
    roadmap_path = ROOT / "docs/implementation/roadmap.md"
    requirements_path = ROOT / "docs/requirements.md"
    evidence_path = ROOT / "docs/research/evidence.md"
    agents_path = ROOT / "AGENTS.md"
    execution_path = ROOT / "docs/spec/execution.md"
    toolchain_path = ROOT / "docs/spec/toolchain.md"

    language = _read(language_path)
    keywords = _extract_word_fence(
        language,
        "2.3 Hard keywords, ordinary words, and reserved sentinels",
    )
    if keywords != EXPECTED_KEYWORDS:
        missing = sorted(EXPECTED_KEYWORDS - keywords)
        extra = sorted(keywords - EXPECTED_KEYWORDS)
        findings.append(
            Finding(language_path, 1, f"keyword drift (missing={missing}, extra={extra})")
        )
    ordinary_match = re.search(
        r"The following removed grammar words are ordinary identifiers:.*?^```text\s*$\n(.*?)^```\s*$",
        language,
        re.MULTILINE | re.DOTALL,
    )
    ordinary = set(ordinary_match.group(1).split()) if ordinary_match else set()
    if ordinary != EXPECTED_REMOVED_GRAMMAR_WORDS:
        missing = sorted(EXPECTED_REMOVED_GRAMMAR_WORDS - ordinary)
        extra = sorted(ordinary - EXPECTED_REMOVED_GRAMMAR_WORDS)
        findings.append(
            Finding(language_path, 1, f"ordinary-word drift (missing={missing}, extra={extra})")
        )
    reserved_match = re.search(
        r"Only `null` and `undefined` are reserved sentinels\.",
        language,
        re.MULTILINE | re.DOTALL,
    )
    reserved = EXPECTED_RESERVED if reserved_match else set()
    if reserved != EXPECTED_RESERVED:
        missing = sorted(EXPECTED_RESERVED - reserved)
        extra = sorted(reserved - EXPECTED_RESERVED)
        findings.append(
            Finding(language_path, 1, f"reserved-word drift (missing={missing}, extra={extra})")
        )

    decisions = _read(decisions_path)
    states, duplicates = _decision_states(decisions)
    for decision, line in duplicates:
        findings.append(Finding(decisions_path, line, f"duplicate decision {decision}"))
    missing_decisions = sorted(REQUIRED_DECISIONS - set(states))
    extra_decisions = sorted(set(states) - REQUIRED_DECISIONS)
    if missing_decisions or extra_decisions:
        findings.append(
            Finding(
                decisions_path,
                1,
                f"decision index drift (missing={missing_decisions}, extra={extra_decisions})",
            )
        )
    for decision, state in states.items():
        if state not in VALID_DECISION_STATES:
            findings.append(
                Finding(decisions_path, 1, f"{decision} has invalid or missing status '{state}'")
            )
    for decision in REQUIRED_PROTOTYPE_DECISIONS:
        section = re.search(
            rf"^##\s+{decision}\b(.*?)(?=^##\s+D\d|\Z)",
            decisions,
            re.MULTILINE | re.DOTALL,
        )
        if not section or not re.search(r"\b(prototype|gate|benchmark|validation)\b", section.group(1), re.I):
            findings.append(
                Finding(decisions_path, 1, f"{decision} lacks an explicit measured gate")
            )

    roadmap = _read(roadmap_path)
    gates = _roadmap_gates(roadmap)
    requirements = _read(requirements_path)
    rows = _requirement_rows(requirements)
    seen_requirements: Set[str] = set()
    for requirement, gate_field, line in rows:
        if requirement in seen_requirements:
            findings.append(Finding(requirements_path, line, f"duplicate requirement {requirement}"))
        seen_requirements.add(requirement)
        referenced_gates = set(re.findall(r"\b[A-Z]\d+\b", gate_field))
        if not referenced_gates:
            findings.append(Finding(requirements_path, line, f"{requirement} has no roadmap gate"))
        for gate in sorted(referenced_gates - gates):
            findings.append(
                Finding(requirements_path, line, f"{requirement} references unknown gate {gate}")
            )
    if not rows:
        findings.append(Finding(requirements_path, 1, "no requirement rows found"))

    canonical_requirements = {
        agents_path: ["The command is `mmay`", "source extension is `.my`", "portable C17"],
        execution_path: ["typed MIR", "default engine for `mmay run`", "default engine for `mmay build`"],
        toolchain_path: ["executable named `mmay`", "`mmay fmt`", "`mmay test`"],
        language_path: ["There is no `async` or `await`", "there is no `impl` declaration"],
    }
    for path, phrases in canonical_requirements.items():
        text = _read(path)
        for phrase in phrases:
            if phrase not in text:
                findings.append(Finding(path, 1, f"missing canonical contract phrase: {phrase}"))

    evidence = _read(evidence_path)
    for heading in ("## Go: evidence and implications", "## Rust: evidence and implications"):
        if heading not in evidence:
            findings.append(Finding(evidence_path, 1, f"missing evidence section: {heading}"))
    primary_domains = ("https://go.dev/", "https://blog.rust-lang.org/", "https://rust-lang.github.io/", "https://openjdk.org/")
    for domain in primary_domains:
        if domain not in evidence:
            findings.append(Finding(evidence_path, 1, f"missing primary-source domain: {domain}"))

    source_surfaces = sorted(
        path for path in (ROOT / "examples").glob("*.my") if path.is_file()
    )
    for markdown in _markdown_files():
        for start_line, block in _fenced_blocks(markdown, "marimay"):
            tokens, scan_findings = _scan(markdown, block)
            for finding in scan_findings:
                findings.append(
                    Finding(markdown, start_line + finding.line - 1, f"Marimay code fence: {finding.message}")
                )
            for token in tokens:
                if token.text in EXPECTED_RESERVED:
                    findings.append(
                        Finding(
                            markdown,
                            start_line + token.line - 1,
                            f"reserved source construct '{token.text}' in Marimay code fence",
                        )
                    )
    if len([path for path in source_surfaces if not path.name.endswith("_test.my")]) < 3:
        findings.append(Finding(ROOT / "examples", 1, "fewer than three readability examples"))

    return findings


def check_example_syntax() -> List[Finding]:
    findings: List[Finding] = []
    examples = sorted(
        path for path in (ROOT / "examples").glob("*.my") if path.is_file()
    )
    if not examples:
        return [Finding(ROOT / "examples", 1, "no .my examples found")]

    for path in examples:
        try:
            text = _read(path)
        except ValueError as error:
            findings.append(Finding(path, 1, str(error)))
            continue
        if "\t" in text:
            findings.append(Finding(path, 1, "tab character is not canonical formatting"))
        if text and not text.endswith("\n"):
            findings.append(Finding(path, len(text.splitlines()), "missing final newline"))
        tokens, scan_findings = _scan(path, text)
        findings.extend(scan_findings)
        for token in tokens:
            if token.text in EXPECTED_RESERVED:
                findings.append(
                    Finding(path, token.line, f"reserved source construct '{token.text}'")
                )

        top_level = {token.text for token in tokens if token.text in {"fn", "type", "enum", "interface", "test", "fuzz"}}
        if not top_level:
            findings.append(Finding(path, 1, "example has no recognizable top-level declaration"))

    return findings


def _tsv_rows(path: Path, width: int) -> Tuple[List[Tuple[int, List[str]]], List[Finding]]:
    rows: List[Tuple[int, List[str]]] = []
    findings: List[Finding] = []
    try:
        text = _read(path)
    except (OSError, ValueError) as error:
        return rows, [Finding(path, 1, str(error))]

    for number, raw_line in enumerate(text.splitlines(), 1):
        if not raw_line or raw_line.startswith("#"):
            continue
        fields = raw_line.split("\t")
        if len(fields) != width:
            findings.append(
                Finding(path, number, f"expected {width} tab-separated fields, found {len(fields)}")
            )
            continue
        rows.append((number, fields))
    return rows, findings


def check_syntax_coverage() -> List[Finding]:
    """Keep syntax-kind and grammar-production matrices complete and auditable.

    An `open` row is deliberately permitted: this check makes every gap visible
    without claiming that I1's production-complete fixture gate has passed.
    """

    findings: List[Finding] = []
    kinds_path = ROOT / "bootstrap/tables/syntax-kinds.tsv"
    coverage_path = ROOT / "tests/syntax/parser/coverage.tsv"
    kind_rows, kind_findings = _tsv_rows(kinds_path, 3)
    coverage_rows, coverage_findings = _tsv_rows(coverage_path, 6)
    findings.extend(kind_findings)
    findings.extend(coverage_findings)

    expected: dict[str, int] = {}
    for line, fields in kind_rows:
        symbol = fields[0]
        if symbol in expected:
            findings.append(Finding(kinds_path, line, f"duplicate syntax kind {symbol}"))
        else:
            expected[symbol] = line

    coverage: dict[str, Tuple[int, List[str]]] = {}
    for line, fields in coverage_rows:
        symbol = fields[0]
        if symbol in coverage:
            findings.append(Finding(coverage_path, line, f"duplicate coverage row {symbol}"))
        else:
            coverage[symbol] = (line, fields)

    for symbol in sorted(set(expected) - set(coverage)):
        findings.append(Finding(coverage_path, 1, f"missing coverage row for {symbol}"))
    for symbol in sorted(set(coverage) - set(expected)):
        line, _fields = coverage[symbol]
        findings.append(Finding(coverage_path, line, f"unknown syntax kind {symbol}"))

    c_test_text = ""
    for path in sorted((ROOT / "tests/c-unit").glob("*.c")):
        try:
            c_test_text += "\n" + _read(path)
        except ValueError as error:
            findings.append(Finding(path, 1, str(error)))

    for symbol in sorted(set(expected) & set(coverage)):
        line, fields = coverage[symbol]
        _kind, positive, formatter, negative_owner, recovery_owner, status = fields
        if status not in {"open", "covered"}:
            findings.append(Finding(coverage_path, line, f"invalid status '{status}' for {symbol}"))

        for surface, reference in (("positive", positive), ("formatter", formatter)):
            if reference == "n/a":
                if symbol != "ERROR":
                    findings.append(
                        Finding(coverage_path, line, f"{surface} may be n/a only for ERROR")
                    )
                continue
            if reference == "-":
                if status == "covered":
                    findings.append(
                        Finding(coverage_path, line, f"covered {symbol} lacks {surface} evidence")
                    )
                continue
            target = (ROOT / reference).resolve()
            try:
                target.relative_to(ROOT)
            except ValueError:
                findings.append(
                    Finding(coverage_path, line, f"{surface} fixture escapes repository: {reference}")
                )
                continue
            if not target.is_file():
                findings.append(
                    Finding(coverage_path, line, f"missing {surface} fixture: {reference}")
                )

        for surface, reference in (
            ("negative", negative_owner),
            ("recovery", recovery_owner),
        ):
            if reference == "-":
                if status == "covered":
                    findings.append(
                        Finding(coverage_path, line, f"covered {symbol} lacks {surface} evidence")
                    )
                continue
            if not reference.startswith("c:") or len(reference) == 2:
                findings.append(
                    Finding(coverage_path, line, f"invalid {surface} owner '{reference}'")
                )
                continue
            owner = reference[2:]
            if re.search(rf"\b{re.escape(owner)}\s*\(", c_test_text) is None:
                findings.append(
                    Finding(coverage_path, line, f"missing C test owner {owner}")
                )

    grammar_path = ROOT / "bootstrap/tables/grammar-productions.tsv"
    production_coverage_path = ROOT / "tests/syntax/parser/production-coverage.tsv"
    grammar_rows, grammar_findings = _tsv_rows(grammar_path, 4)
    production_rows, production_findings = _tsv_rows(production_coverage_path, 7)
    findings.extend(grammar_findings)
    findings.extend(production_findings)

    expected_productions: dict[str, Tuple[int, str]] = {}
    for line, fields in grammar_rows:
        name, _category, state, _rule = fields
        if name in expected_productions:
            findings.append(Finding(grammar_path, line, f"duplicate production {name}"))
        else:
            expected_productions[name] = (line, state)

    production_coverage: dict[str, Tuple[int, List[str]]] = {}
    for line, fields in production_rows:
        name = fields[0]
        if name in production_coverage:
            findings.append(
                Finding(production_coverage_path, line, f"duplicate coverage row {name}")
            )
        else:
            production_coverage[name] = (line, fields)

    for name in sorted(set(expected_productions) - set(production_coverage)):
        findings.append(
            Finding(production_coverage_path, 1, f"missing production coverage row for {name}")
        )
    for name in sorted(set(production_coverage) - set(expected_productions)):
        line, _fields = production_coverage[name]
        findings.append(
            Finding(production_coverage_path, line, f"unknown grammar production {name}")
        )

    for name in sorted(set(expected_productions) & set(production_coverage)):
        line, fields = production_coverage[name]
        (
            _production,
            positive,
            formatter,
            negative_owner,
            recovery_owner,
            parser_owner,
            status,
        ) = fields
        if status not in {"open", "covered"}:
            findings.append(
                Finding(
                    production_coverage_path,
                    line,
                    f"invalid status '{status}' for production {name}",
                )
            )

        for surface, reference in (("positive", positive), ("formatter", formatter)):
            if reference == "-":
                if status == "covered":
                    findings.append(
                        Finding(
                            production_coverage_path,
                            line,
                            f"covered production {name} lacks {surface} evidence",
                        )
                    )
                continue
            target = (ROOT / reference).resolve()
            try:
                target.relative_to(ROOT)
            except ValueError:
                findings.append(
                    Finding(
                        production_coverage_path,
                        line,
                        f"{surface} fixture escapes repository: {reference}",
                    )
                )
                continue
            if not target.is_file():
                findings.append(
                    Finding(
                        production_coverage_path,
                        line,
                        f"missing {surface} fixture: {reference}",
                    )
                )

        for surface, reference in (
            ("negative", negative_owner),
            ("recovery", recovery_owner),
            ("parser", parser_owner),
        ):
            if reference == "-":
                if status == "covered":
                    findings.append(
                        Finding(
                            production_coverage_path,
                            line,
                            f"covered production {name} lacks {surface} evidence",
                        )
                    )
                continue
            if not reference.startswith("c:") or len(reference) == 2:
                findings.append(
                    Finding(
                        production_coverage_path,
                        line,
                        f"invalid {surface} owner '{reference}'",
                    )
                )
                continue
            owner = reference[2:]
            if re.search(rf"\b{re.escape(owner)}\s*\(", c_test_text) is None:
                findings.append(
                    Finding(production_coverage_path, line, f"missing C test owner {owner}")
                )

    return findings


def _syntax_coverage_summary() -> str:
    kind_rows, _kind_findings = _tsv_rows(
        ROOT / "tests/syntax/parser/coverage.tsv",
        6,
    )
    production_rows, _production_findings = _tsv_rows(
        ROOT / "tests/syntax/parser/production-coverage.tsv",
        7,
    )
    kind_covered = sum(fields[5] == "covered" for _line, fields in kind_rows)
    production_covered = sum(
        fields[6] == "covered" for _line, fields in production_rows
    )
    return (
        f"syntax kinds: {kind_covered} covered, {len(kind_rows) - kind_covered} open; "
        f"grammar productions: {production_covered} covered, "
        f"{len(production_rows) - production_covered} open"
    )


def _run(name: str, check: callable) -> int:
    findings = check()
    if findings:
        for finding in sorted(findings, key=lambda item: (str(item.path), item.line, item.message)):
            print(finding.render(), file=sys.stderr)
        print(f"{name}: FAILED ({len(findings)} finding(s))", file=sys.stderr)
        return 1
    if name == "check-syntax-coverage":
        print(f"{name}: ok ({_syntax_coverage_summary()})")
    else:
        print(f"{name}: ok")
    return 0


def main(check_name: str) -> int:
    checks = {
        "doc-links": ("check-doc-links", check_doc_links),
        "spec-terms": ("check-spec-terms", check_spec_terms),
        "example-syntax": ("check-example-syntax", check_example_syntax),
        "syntax-coverage": ("check-syntax-coverage", check_syntax_coverage),
    }
    if check_name not in checks:
        print(f"unknown D0 check: {check_name}", file=sys.stderr)
        return 2
    display, check = checks[check_name]
    return _run(display, check)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(
            "usage: _d0_checks.py doc-links|spec-terms|example-syntax|syntax-coverage",
            file=sys.stderr,
        )
        raise SystemExit(2)
    raise SystemExit(main(sys.argv[1]))
