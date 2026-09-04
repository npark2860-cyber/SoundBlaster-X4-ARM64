# DEBUG HISTORY — ASIO B5 48 kHz PASS / 96 kHz scheduling fix

Date: 2026-09-04 KST

## Returned runtime report

Report generated 2026-09-04 11:29:00 on the X4 ARM64EC test system.

Registration / contract remained proven:

- B5 side-by-side registration PASS
- registry verification PASS
- initial property-only Render Pin 1 gate `C 0/1 G 0/1`, BUSY=NO
- B5 COM/public capability PASS
- 2 inputs / 2 outputs
- all exposed channels Int24LSB type 17
- buffer 96..4800 / preferred 240 / granularity 48
- 48/96/192 kHz output contract
- 48/96 kHz capture contract
- latency 240/240 at preferred size
- ASIO 2.x time-info support

## Runtime progress proven

### 48 kHz / 240 frames / output-only

Three cycles passed cleanly.

Observed callback counts:

- 140
- 139
- 142

Every cycle reported:

- stop=ASE_OK
- workerJoined=YES
- no callback-index errors
- no render packet discontinuities
- no copy errors

### 48 kHz / 240 frames / full duplex

Two cycles passed cleanly.

Cycle 1:

- callbacks=139
- renderNotif=140
- captureNotif=139
- outFrames=33360
- inFrames=33360
- stop=ASE_OK

Cycle 2:

- callbacks=138
- renderNotif=139
- captureNotif=138
- outFrames=33120
- inFrames=33120
- stop=ASE_OK

`inputNonzeroSamples=0` in this silent validation does not prove real microphone/line content. It only proves the capture lifecycle/copy path survived the matrix. A final real-use input test remains required later.

## Failure narrowed to 96 kHz scheduling

First `preferred-96-duplex` cycle reached RUN and processed 259 callbacks, but stop returned ASE_HWMalfunction because strict diagnostics observed:

- worker_failed = 0
- callback_index_errors = 20
- render_copy_errors = 0
- capture_copy_errors = 0
- render_packet_discontinuities = 20
- render_position_regressions = 0
- capture_packet_discontinuities = 0

This is not a BUSY failure, pin-creation failure, KS state failure, DMA copy failure, or capture packet failure.

The runtime trace shows the render `PACKETCOUNT` eventually skipped absolute packet numbers while capture packet numbers remained sequential. Because callback slot selection derives from `(renderPacket + 1) % 2`, a +2 packet jump repeats the same host buffer index and increments the callback-index diagnostic in lockstep with the render discontinuity count.

At 96 kHz / 240 frames the render callback period is 2.5 ms. At 192 kHz / 240 frames it is 1.25 ms. The initial B5 worker used a normal `CreateThread` and emitted per-notification diagnostics from the realtime path.

## Fix implemented

B5 branch was advanced on the same productization line. Validated B4D remains untouched.

The ARM64EC and Classic ARM64 B5 driver adapters now wrap the shared worker start routine in a Windows MMCSS trampoline:

- `AvSetMmThreadCharacteristicsW(L"Pro Audio", ...)`
- `AvSetMmThreadPriority(..., AVRT_PRIORITY_CRITICAL)`
- fallback only if MMCSS registration fails: `THREAD_PRIORITY_HIGHEST`
- `AvRevertMmThreadCharacteristics(...)` before thread exit

B5 targets link `avrt.lib`.

To keep existing detailed diagnostics without performing file I/O in the realtime packet loop, the B5 DLL static CRT stdout is placed on a 2 MiB full buffer at DLL initialization and flushed after the worker loop exits.

Strict validation remains unchanged:

- render packet discontinuity is still fatal
- capture packet discontinuity is still fatal
- callback index repetition is still fatal
- copy errors are still fatal
- BUSY is still never bypassed

The fix therefore does not turn the observed 96 kHz failure into a false PASS; it removes the scheduling/logging pressure and requires the exact same strict matrix to pass afterward.

## Next action

Rebuild `Build ASIO B5 Productization` from the current B5 branch.

Do not reuse the old DLL for the 96 kHz retest because the MMCSS adapter is in the new DLL.

After Actions PASS, run the bundled `install_and_validate_b5.cmd` once and return the report.

The next report must prove the full matrix through:

- 48 kHz preferred output
- 48 kHz preferred full duplex
- 96 kHz preferred full duplex
- 192 kHz preferred output
- 48 kHz minimum 96-frame output
- 48 kHz maximum 4800-frame output
- 48 kHz 512-frame compatibility output

Only after a full strict matrix PASS should final REAPER audible-output + real stereo-input validation be performed.
