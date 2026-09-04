# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated fallback

B4D remains frozen and proven:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Never bypass BUSY gates, never intentionally recreate the historical active-render collision, and never tear hardware down before the worker is joined.

---

# Current B5 source

Current branch HEAD:

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

Latest runtime-code commit:

`64e34b48714789ab17fba57be34b054f2170b4e9`

The branch HEAD after that commit only updates the B5 productization README.

Required runtime/build markers:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`
- `post-coalesce-stale-v1`

---

# Latest product validation — PASS

Report generated `2026-09-04 16:16:13.09` returned:

`B5 INSTALL + PRODUCT VALIDATION: PASS`

Key cases:

- 48k/240 output x3 PASS
- 48k/240 duplex x2 PASS
- 96k/240 duplex x2 PASS
- REAPER-matched 48k/480 output / 5 s PASS with 500 callbacks, stop=0, workerJoined=YES
- 192k/384 output x2 PASS
- 48k/96 PASS
- 48k/4800 PASS
- 48k/512 compatibility PASS
- ASIO public capability probe PASS

The repeated allocation-only 192 kHz geometry probe remains unchanged: 384 frames is the first accepted geometry; 432..960 are accepted. Do not rerun geometry unless contradictory evidence appears.

---

# Latest cadence edge case

The dedicated 192 kHz cadence run reproduced a real +2 coalesce and proved the first mux-v4 recovery step works.

At 432 frames:

`1941 -> 1943`

was recovered as:

`B5 RENDER COALESCE RECOVERED previous=1941 missed=1942 current=1943 droppedBlocks=1`

The immediately following render wake returned the same current packet again:

`1943 -> 1943`

and the old strict duplicate rule killed the worker.

The same pattern occurred at 480 frames:

`3485 -> 3487` recovered, then immediate duplicate `3487 -> 3487` fatal.

This is narrower than globally tolerating duplicates. See:

`DEBUG_HISTORY_20260904_ASIO_B5_POST_COALESCE_STALE_WAKE.md`

---

# Implemented fix — one-shot post-coalesce stale wake

After a successful forward Render `delta == 2` recovery only, mux-v4 arms exactly one allowance for the immediately following render wake.

If that next wake reports the same PACKETCOUNT:

- it is classified as `post-coalesce-stale-v1`;
- no ASIO callback is issued;
- no WaveRT render write occurs;
- callback/sample timeline is not advanced;
- `renderStaleWakes` increments;
- the one-shot allowance clears;
- streaming continues.

If the next wake advances normally, the allowance clears without special handling.

Still fatal:

- any unarmed duplicate
- a second duplicate after the one consumed stale wake
- backward Render PACKETCOUNT
- Render forward delta >2
- Capture packet discontinuity
- render position regression
- callback-index/copy/staging/join failure

`runtime-failsafe-v1` remains the fatal fallback and still performs silence before failure logging, never worker-side pin teardown.

---

# Immediate action

1. run manual workflow `Build ASIO B5 Productization`;
2. require ARM64EC + Classic ARM64 compile/link PASS;
3. require all four runtime markers above in both DLLs;
4. install the new ZIP with all other X4 clients closed;
5. **do not rerun geometry**;
6. run `probe_b5_192k_cadence.cmd` once because it directly reproduced this edge case;
7. return `B5_192K_CADENCE_REPORT.txt`.

Desired recovery evidence if a +2 occurs:

- `B5 RENDER COALESCE RECOVERED ...`
- optionally `B5 RENDER POST-COALESCE STALE WAKE consumed ...`
- worker remains alive
- strict packet/position/index/copy counters remain zero
- stop=0 / workerJoined=YES

If cadence passes, run `install_and_validate_b5.cmd` once as the final shared-worker regression check. Do not repeatedly stress the device after a fatal result.

---

# After runtime closure

If cadence + final product matrix pass, perform one ordinary REAPER audible test at the already-observed host geometry:

- 48 kHz
- 24-bit
- 2 in / 2 out
- 480 samples

If that is stable, resume the binding native ASIO control-panel milestone:

- own Win32 UI, no Creative binary reuse
- `IASIO::controlPanel()`
- current/effective sample rate and buffer
- latency frames + milliseconds
- safe persistence/reopen/reset behavior
- no WaveRT pin creation merely from opening the panel
- no live mutation while buffers/RUN are active
- deterministic Apply/OK/Cancel
- lightweight diagnostics/save-report support

After panel PASS, finish real output + real stereo input validation at 48/96 kHz, freeze B5 first release, then resume deferred CTCDC/CTIntrfu work.