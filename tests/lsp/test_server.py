#!/usr/bin/env python3
"""Exercise the public mmay LSP subprocess, including its byte framing."""

import json
import queue
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


TIMEOUT = 5
MAX_MESSAGE = 16 * 1024 * 1024


class Client:
    def __init__(self, binary, directory):
        self.process = subprocess.Popen(
            [str(binary), "lsp"], cwd=directory,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.messages = queue.Queue()
        self.pending = []
        self.next_id = 1
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        try:
            while True:
                headers = {}
                while True:
                    line = self.process.stdout.readline(8193)
                    if not line:
                        raise EOFError("language server closed stdout")
                    if line == b"\r\n":
                        break
                    assert line.endswith(b"\r\n") and len(line) <= 8192, line
                    name, value = line.decode("ascii").split(":", 1)
                    headers[name.lower()] = value.strip()
                length = int(headers["content-length"])
                assert 0 < length <= MAX_MESSAGE, length
                body = self.process.stdout.read(length)
                assert len(body) == length, "truncated response"
                message = json.loads(body)
                assert message.get("jsonrpc") == "2.0", message
                self.messages.put(message)
        except Exception as error:  # Deliver reader failures to the bounded main thread.
            self.messages.put(error)

    def raw(self, body):
        self.process.stdin.write(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii"))
        self.process.stdin.write(body)
        self.process.stdin.flush()

    def send(self, method, params=None, request=False):
        message = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            message["params"] = params
        identifier = None
        if request:
            identifier = self.next_id
            self.next_id += 1
            message["id"] = identifier
        self.raw(json.dumps(message, ensure_ascii=False).encode("utf-8"))
        return identifier

    def receive(self, predicate):
        for index, message in enumerate(self.pending):
            if predicate(message):
                return self.pending.pop(index)
        deadline = time.monotonic() + TIMEOUT
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError(f"timed out; pending messages: {self.pending!r}")
            try:
                message = self.messages.get(timeout=remaining)
            except queue.Empty as error:
                raise AssertionError(f"LSP response timeout; pending: {self.pending!r}") from error
            if isinstance(message, Exception):
                raise AssertionError(f"LSP reader failed: {message}") from message
            if predicate(message):
                return message
            self.pending.append(message)

    def request(self, method, params=None, error=None):
        identifier = self.send(method, params, request=True)
        response = self.receive(lambda item: item.get("id") == identifier)
        if error is not None:
            assert response.get("error", {}).get("code") == error, response
            return response
        assert "error" not in response and "result" in response, response
        return response["result"]

    def diagnostics(self, uri, version):
        message = self.receive(lambda item:
            item.get("method") == "textDocument/publishDiagnostics"
            and item["params"].get("uri") == uri
            and item["params"].get("version") == version)
        diagnostics = message["params"]["diagnostics"]
        for diagnostic in diagnostics:
            assert diagnostic["message"] and diagnostic["code"], diagnostic
            assert diagnostic["source"] == "mmay", diagnostic
            assert diagnostic["range"]["start"]["line"] >= 0, diagnostic
            assert diagnostic["range"]["start"]["character"] >= 0, diagnostic
        return diagnostics

    def open(self, path, text, version=1):
        uri = path.as_uri()
        self.send("textDocument/didOpen", {"textDocument": {
            "uri": uri, "languageId": "marimay", "version": version, "text": text,
        }})
        return self.diagnostics(uri, version)

    def change(self, path, text, version):
        self.send("textDocument/didChange", {
            "textDocument": {"uri": path.as_uri(), "version": version},
            "contentChanges": [{"text": text}],
        })

    def finish(self):
        assert self.request("shutdown") is None
        self.send("exit")
        assert self.process.wait(timeout=TIMEOUT) == 0

    def dispose(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait(timeout=TIMEOUT)
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            stream.close()
        self.reader.join(timeout=TIMEOUT)


def position(source, needle):
    offset = source.index(needle)
    prefix = source[:offset]
    return {
        "line": prefix.count("\n"),
        "character": len(prefix.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2,
    }


def document_position(path, source, needle):
    return {"textDocument": {"uri": path.as_uri()}, "position": position(source, needle)}


def offset_at(source, point):
    lines = source.splitlines(keepends=True)
    prefix = "".join(lines[:point["line"]])
    line = lines[point["line"]] if point["line"] < len(lines) else ""
    units = 0
    for index, character in enumerate(line):
        if units == point["character"]:
            return len(prefix) + index
        units += len(character.encode("utf-16-le")) // 2
    assert units == point["character"], point
    return len(prefix) + len(line)


def apply_edits(source, edits):
    replacements = [(offset_at(source, edit["range"]["start"]),
                     offset_at(source, edit["range"]["end"]), edit["newText"])
                    for edit in edits]
    for start, end, text in sorted(replacements, reverse=True):
        source = source[:start] + text + source[end:]
    return source


def initialize(client, root):
    result = client.request("initialize", {
        "processId": None, "rootUri": root.as_uri(),
        "capabilities": {"general": {"positionEncodings": ["utf-16"]}},
    })
    capabilities = result["capabilities"]
    assert capabilities.get("positionEncoding", "utf-16") == "utf-16", result
    sync = capabilities["textDocumentSync"]
    assert (sync.get("change") if isinstance(sync, dict) else sync) == 1, result
    for name in ("definitionProvider", "hoverProvider", "documentSymbolProvider",
                 "referencesProvider", "documentFormattingProvider"):
        assert capabilities.get(name), (name, result)
    client.send("initialized", {})


def check_documents(client, root):
    standalone = root / "standalone.my"
    valid = "fn main(): int { return 1; }\n"
    standalone.write_text(valid, encoding="utf-8")
    lexical = 'fn main(): int { return "unterminated; }\n'
    assert client.open(standalone, lexical), "missing lexical diagnostics"
    for version, text in (
        (2, "fn main(: int { return 1; }\n"),
        (3, "fn main(): int { return true; }\n"),
    ):
        client.change(standalone, text, version)
        assert any(d["severity"] == 1 for d in client.diagnostics(
            standalone.as_uri(), version)), "missing parser/checker error"
    client.change(standalone, valid, 4)
    assert client.diagnostics(standalone.as_uri(), 4) == []
    client.change(standalone, lexical, 3)  # Older overlays must not replace version 4.
    symbols = client.request("textDocument/documentSymbol", {
        "textDocument": {"uri": standalone.as_uri()},
    })
    assert any(symbol["name"] == "main" for symbol in symbols), symbols
    # A new valid revision also proves that the ignored stale update did not poison state.
    client.change(standalone, valid, 5)
    assert client.diagnostics(standalone.as_uri(), 5) == []
    assert not any(item.get("method") == "textDocument/publishDiagnostics"
                   and item["params"].get("uri") == standalone.as_uri()
                   and item["params"].get("version", 5) < 4
                   for item in client.pending), client.pending
    client.send("textDocument/didClose", {"textDocument": {"uri": standalone.as_uri()}})
    cleared = client.receive(lambda item:
        item.get("method") == "textDocument/publishDiagnostics"
        and item["params"].get("uri") == standalone.as_uri())
    assert cleared["params"]["diagnostics"] == [], cleared

    unicode_file = root / "ошибка %20 space.my"
    source = "/* 😀 */ fn main(): int { return missingValue; }\n"
    unicode_file.write_text(valid, encoding="utf-8")
    errors = client.open(unicode_file, source)
    expected = position(source, "missingValue")
    assert any(d["range"]["start"] == expected for d in errors), (expected, errors)
    client.send("textDocument/didClose", {"textDocument": {"uri": unicode_file.as_uri()}})


def check_navigation(client, root):
    module = root / "module with space %"
    module.mkdir()
    (module / "mmay.mod").write_text('module "example/lsp-test"\n\nlanguage "0.1"\n',
                                     encoding="utf-8")
    provider = module / "определение.my"
    consumer = module / "consumer.my"
    provider_source = "fn answer():int{return 42;}\n"
    consumer_source = "fn main(): int { /* 😀 */ return answer(); }\n"
    provider.write_text(provider_source, encoding="utf-8")
    consumer.write_text(consumer_source, encoding="utf-8")
    assert client.open(consumer, consumer_source) == []
    request = document_position(consumer, consumer_source, "answer")
    definitions = client.request("textDocument/definition", request)
    if isinstance(definitions, dict):
        definitions = [definitions]
    assert definitions and definitions[0]["uri"] == provider.as_uri(), definitions
    assert definitions[0]["range"]["start"]["line"] == 0, definitions

    hover = client.request("textDocument/hover", request)
    assert hover and hover.get("contents"), hover
    assert "answer" in json.dumps(hover) or "int" in json.dumps(hover), hover
    references = client.request("textDocument/references", {
        **request, "context": {"includeDeclaration": True},
    })
    assert {item["uri"] for item in references} == {
        provider.as_uri(), consumer.as_uri()}, references
    use = next(item for item in references if item["uri"] == consumer.as_uri())
    assert use["range"]["start"] == position(consumer_source, "answer"), use
    references = client.request("textDocument/references", {
        **request, "context": {"includeDeclaration": False},
    })
    assert references and all(item["uri"] == consumer.as_uri() for item in references), references

    assert client.open(provider, provider_source) == []
    symbols = client.request("textDocument/documentSymbol", {
        "textDocument": {"uri": provider.as_uri()},
    })
    assert any(symbol["name"] == "answer" for symbol in symbols), symbols
    edits = client.request("textDocument/formatting", {
        "textDocument": {"uri": provider.as_uri()},
        "options": {"tabSize": 2, "insertSpaces": True},
    })
    assert edits, edits
    assert apply_edits(provider_source, edits) == "fn answer(): int {\n  return 42;\n}\n", edits


def check_transport(binary, root):
    for name, wire, success in (
        ("clean EOF", b"", True),
        ("truncated header", b"Content-Length: 2\r\n", False),
        ("truncated body", b"Content-Length: 10\r\n\r\n{}", False),
        ("oversized body", f"Content-Length: {MAX_MESSAGE + 1}\r\n\r\n".encode(), False),
        ("oversized header", b"X-Header: " + b"x" * 8192 + b"\r\n\r\n", False),
    ):
        process = subprocess.Popen([str(binary), "lsp"], cwd=root,
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        try:
            process.communicate(wire, timeout=TIMEOUT)
            assert (process.returncode == 0) == success, (name, process.returncode)
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate(timeout=TIMEOUT)


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="mmay-lsp-") as temporary:
        root = Path(temporary).resolve()
        client = Client(binary, root)
        try:
            client.raw(b'{"jsonrpc":')
            parsed = client.receive(lambda item: item.get("id") is None and "error" in item)
            assert parsed["error"]["code"] == -32700, parsed
            client.request("textDocument/hover", {}, error=-32002)
            initialize(client, root)
            client.request("marimay/unknown", {}, error=-32601)
            check_documents(client, root)
            check_navigation(client, root)
            client.finish()
        finally:
            client.dispose()
        check_transport(binary, root)
    print("LSP subprocess protocol, diagnostics, navigation, formatting and UTF-16: ok")


if __name__ == "__main__":
    main()
