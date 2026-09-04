# DEBUG HISTORY — ASIO B5 product validation runtime BUSY race

Date: 2026-09-04 KST

## Runtime report

User returned `B5_PRODUCT_VALIDATION_REPORT.txt` generated 2026-09-04 11:24:32.

The package successfully registered and loaded the side-by-side B5 driver:

- `B5 REGISTER: PASS`
- registry verify PASS
- B5 DLL loaded from `C:\SB\x4-asio-arm64ec-b5.dll`
- B5 CLSID `{4BDF4DA4-27F8-47C3-BBEC-6A745C115B49}`

Initial property-only KS gate was FREE:

- Render Pin 1 `C 0/1`
- Render Pin 1 `G 0/1`
- BUSY = NO
- property-only probe never called `KsCreatePin`

B5 public ASIO capability report then passed:

- driver version 200
- init FREE
- 2 inputs / 2 outputs at 48 kHz
- Int24LSB type 17 on all four exposed channels
- buffer 96..4800 / preferred 240 / granularity 48
- supported rates 48 / 96 / 192 kHz only
- Internal Clock
- latency 240 in / 240 out
- time-info supported

Immediately after that capability process completed, the bundled product lifecycle matrix began and its first `init()` observed:

- Render Pin 1 `C 0/1`
- Render Pin 1 `G 1/1`
- BUSY
- `KsCreatePin` skipped

Result:

`B5 PRODUCT VALIDATION RESULT: BUSY_BLOCKED`

The wrapper then labeled the overall run FAIL even though the driver correctly enforced the safety gate.

## Analysis

The B5 capability probe `report()` calls `init()` and public query APIs only. It does not call `createBuffers()`, `start()`, or `KsCreatePin`, and it releases the COM driver before exit.

Therefore this report does not support attributing the `G 1/1` owner to the B5 capability probe itself.

The observed transition is a cross-process ownership race/window:

1. initial KS property gate saw `G 0/1`;
2. public capability process ran without creating a pin;
3. before the lifecycle matrix first init, some other filter/process context acquired the single global Render Pin 1 instance;
4. B5 correctly refused to create a pin.

Do not bypass BUSY and do not classify this as a transport crash/regression.

## Script fix

B5 branch advanced with an updated `install_and_validate_b5.cmd` that:

1. registers/verifies B5;
2. runs the immutable property-only idle gate;
3. **immediately runs the actual lifecycle matrix while the FREE window is current**;
4. treats lifecycle exit code 10 as `BUSY_BLOCKED_DURING_MATRIX`, not generic FAIL;
5. records one post-block property-only KS snapshot without creating a pin;
6. moves the public capability report after a successful lifecycle matrix.

This reduces the unnecessary inter-process gap between the proven FREE gate and the first actual B5 pin acquisition.

## Existing package can still be used

No new binary behavior is required for the immediate next runtime attempt. The already-packaged `x4-asio-stage-b5-product-validation.exe` performs its own B5 `init()` safety gate before any pin creation.

A single direct execution of that EXE, after making X4 idle, is sufficient to test the missing lifecycle matrix without rerunning the preceding public capability process.

If the direct matrix again immediately reports `G 1/1`, do not repeatedly retry. Proceed to ownership diagnostics instead.

## Immutable safety

Never bypass Render Pin 1 local/global BUSY.

Never intentionally reproduce the historical active-render collision:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` recovery path
