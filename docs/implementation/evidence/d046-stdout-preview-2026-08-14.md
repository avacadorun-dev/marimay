# D046 preview user stdout — documentation checkpoint

Date: 2026-08-14  
Gate: I5/I6 standard I/O preview contract, not engine implementation  
Status: accepted decision and normative specs; engines not implemented

Authority: accepted [D046](../../design/decisions.md),
[standard-library §3.1](../../spec/standard-library.md),
[execution §10.1 and §13.1](../../spec/execution.md),
[toolchain run/test/exec grants](../../spec/toolchain.md).

## Closed in this change

- Public write surface is `import { println } from "std/io"` with signature
  `fn println(text: string): Result<void, io.Error>`.
- Allowed argument type is only `string`; no implicit conversion and no
  D003 multi-type overload.
- Binary32/binary64 text is `std/convert.formatF32`/`formatF64` under the
  D046 unique ASCII algorithm.
- Newline is exactly one LF. Bytes are the string's UTF-8, including NUL.
- Capability `"io.stdout"` is granted by default for local `ruda run` and
  native `ruda build` artifacts, captured by default in `ruda test`, and
  denied for untrusted `ruda exec`.
- Success is line-atomic publish-then-observe. VM, native, and test capture
  compare those bytes. The internal `ok:f64:` hex harness is not user stdout.

## Not implemented here

No checker resolution of `std/io`, no MIR/VM/native write opcode, and no
executable `ruda run`/`ruda test` capture of this surface. Those belong to
the next engine checkpoint. Parser and formatter fixtures plus user examples
own the public source shape now.
