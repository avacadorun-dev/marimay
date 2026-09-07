# Gitea recovery and bounded CI concurrency

The user explicitly authorized recovery, virtual-disk expansion and delegation
of infrastructure work. A separate infrastructure agent performed this work;
these observations do not close a language checkpoint or platform gate.

## Recovery observations

On 2026-09-08 VM 106 was recovered with a 64 GiB virtual disk. The original
19 GiB root filesystem had reached 100%, preventing PostgreSQL from creating
`postmaster.pid`. The expanded root partition is 67,644,686,336 bytes; the ext4
filesystem is 66,394,636,288 bytes, with 48,322,596,864 bytes available at the
final check. Root and swap UUIDs were preserved. Swap is active and unused.
Filesystem mount verification reported no errors; Gitea, PostgreSQL and the
QEMU guest agent were healthy. Git read and push succeeded.

One ordinary runner remains active. Two other ordinary runners and the
performance runner are manually stopped under `restart=unless-stopped`.
This persists across restart, but an explicit Compose start/recreation can
start them again. Recovery snapshots remain; none was deleted or rolled back.

## Results and unresolved failures

Run 210 used `8e6fe1cba4378a136644bdc523fbd4265ded5328`. Following the earlier
RCU stall, all four 600-second fuzz jobs completed. The sequential Clang Debug
retry passed. The ASan/UBSan/LSan retry ran 75 tests: 71 passed and four failed
with Clang/linker errors. Diagnostics include a linker internal error, a PLT
offset overflow, a Clang frontend crash and the token `anvironment` where the
committed source contains `environment`. Artifact upload succeeded.

During the heavy retries, services remained responsive, swap remained unused,
and no new RCU/OOM/watchdog/I/O error was observed. The sampled maximum runnable
queue was 220 despite only one runner. Neither this queue nor the successful
recovery proves the cause of the earlier stall or compiler failures.

Sanitized job logs and the reconstructed sanitizer artifact remain under
`build/gitea-recovery-2026-09-07/`. The retained `LastTest.log` SHA-256 is
`44429130553fa3afdbb3ab89387f20ee28e0e84feac4376a61a2acf812778dcf`.
Clang reported failure to generate its preprocessed reproducer. The failed
task's transient checkout and temporary compiler directory were already gone.
Expected local and server Git blobs match for the implicated files; this does
not establish which bytes the failed compiler actually read. Read-only host,
guest and NVMe checks found no relevant machine-check/storage error; these
checks cannot exclude transient hardware faults. Both causes remain unknown.

Documentation-only run 211 on `a95300fc1d3df7370e6f1c2a005f75bc01b29662`
passed classification and documentation/architecture jobs. Code, sanitizer and
fuzz jobs were skipped, so this is not compiler acceptance evidence.

## Workflow containment

The accompanying workflow change bounds all five CMake build invocations at
four jobs. ASan and TSan CTest use `min(nproc, 2)`; ordinary CTest remains at
four. Test membership, all allocation-failure indices, sanitizer options,
four sustained fuzz durations and artifact capture are unchanged. Local
presets are unchanged. One runner remains the infrastructure setting while
the bounded workflow awaits a fresh heavy CI result.

The [machine-readable observations](gitea-recovery-2026-09-08.json) retain the
recovery metrics and historical run status. This mitigation reduces concurrent
subprocess pressure; it does not repair or explain historical failures.
CP42, CP41 and the release gates remain open. No book idea was used here.
