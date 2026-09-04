# DEBUG HISTORY — ASIO B5 mux v2 ARM64EC cguid compile fix

Date: 2026-09-04 KST

## Failing build

Manual workflow command:

```text
cmake --build build/asio-b5-arm64ec --config Release --target x4-asio-arm64ec-b5 x4-asio-stage-b5-register x4-asio-stage-b5-product-validation x4-asio-stage-b5-capability-probe x4-asio-stage-b5-ks-probe --parallel
```

MSBuild 17.14.51 / Windows SDK 10.0.26100.0 failed while compiling `driver_b5_arm64ec.cpp`:

```text
Windows Kits\10\Include\10.0.26100.0\um\cguid.h(33,18):
error C2059: syntax error: '__uuidof'
```

The B5 DLL did not compile, so there is no hardware/runtime conclusion from this run.

## Root cause

The first dual-event mux adapter exposed WaveRT/driver internals with:

```cpp
#define private public
#include "asio_callback_compat.h"
#include "b5_identity.h"
#include "preflight.h"
#include "wavert_engine_b5.h"
#undef private
```

That allowed the C++ keyword macro to reach Windows/COM SDK declarations pulled through the project headers. The failure surfaced in `cguid.h` at `__uuidof`.

This is an adapter/preprocessor integration failure, not a WaveRT, BUSY, or hardware failure.

## Fix

### 1. Windows/COM/project headers are now parsed normally

ARM64EC and Classic driver adapters include all SDK/project headers before any translation-unit-local driver access macro.

The SDK can no longer see the `private` replacement.

### 2. WaveRT engine private access was removed from the mux

`wavert_engine_b5.h` now exposes a narrow B5-only API:

```cpp
process_signaled_notification(...)
```

The dual-event mux calls it only after `WaitForMultipleObjects` has already observed that engine's notification event.

Implementation lives in:

`src/asio-arm64-stage-b0/wavert_engine_b5_signaled.inl`

and is included by both ARM64EC and Classic architecture adapters.

### 3. Capture NOT_READY semantics retained

The signaled API maps capture `ERROR_NOT_READY` to `X4WaveRtB5ProcessResult::NoData` rather than a hard worker failure.

Actual capture packet discontinuity remains strict/fatal through the public stats counters.

### 4. Mux worker advanced to v2

Runtime marker is now:

`dual-event-mux-v2`

The mux no longer reads WaveRT private fields or raw pin handles. It uses:

- public `notification_event()`
- public `process_signaled_notification()`
- public `stats()`
- public capture/render copy methods

Strict failures remain:

- render packet discontinuity
- capture packet discontinuity
- render position regression
- repeated ASIO callback buffer index
- render/capture copy failure
- duplex packet/slot synchronization mismatch

BUSY is unchanged and must never be bypassed.

### 5. Workflow binary marker updated

`Build ASIO B5 Productization` now requires `dual-event-mux-v2` to be physically present in both built DLLs before packaging.

If either binary lacks the marker, Actions fails before ZIP creation.

## Current B5 source after fix

`exp/windows-arm64-asio-b5-capability-productization@869307d44750af3e23c2de68dc84cc32d9b5e05f`

Validated B4D comparison:

- ahead: 41
- behind: 0
- merge base: `a95a95d014bcc1c3a521be41325841ae96dc8a61`
- validated B4D core remains unchanged

## Next action

Re-run `Build ASIO B5 Productization`.

Do not perform another hardware validation until:

1. ARM64EC B5 DLL compiles/links;
2. all helper targets compile;
3. Classic ARM64 B5 compiles/links;
4. PE architecture checks pass;
5. both DLLs contain `dual-event-mux-v2`;
6. productization ZIP is produced.

If build fails, continue fixing the exact compiler/linker/workflow error on this same B5 branch. Do not create a microbranch or request a hardware micro-test.
