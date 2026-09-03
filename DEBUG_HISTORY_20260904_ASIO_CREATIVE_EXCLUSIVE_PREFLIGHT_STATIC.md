# DEBUG HISTORY — Creative x64 ASIO coexistence / exclusive-preflight static trace

Date: 2026-09-04 KST

## Scope

Static analysis of the user-supplied Creative x64 ASIO reference binary `CtU2As64.DLL` after the active-playback crash trigger was isolated on Windows ARM64.

Reference-only binary identity:

- file: `CtU2As64.DLL`
- size: `226,816` bytes
- SHA-256: `2264bf3f8d7d9b85a07950654c672f550ce1b703ae433b24c89578b866195766`
- PE32+ x86-64
- PE timestamp: 2025-02-04 02:17:39 UTC

The binary is not committed to this repository and must not become a runtime dependency.

## Highest-value finding

Creative does not blindly call `KsCreatePin` when the target KS pin is already out of available instances.

Before pin instantiation, it checks both:

- `KSPROPERTY_PIN_CINSTANCES` — property ID `0`
- `KSPROPERTY_PIN_GLOBALCINSTANCES` — property ID `8`

using `KSPROPSETID_Pin` `{8C134960-51AD-11CF-878A-94F801C10000}` and `IOCTL_KS_PROPERTY`.

The helper at VA `0x4152F0` returns busy when either returned `KSPIN_CINSTANCES` structure has:

`CurrentCount >= PossibleCount`.

This check is performed immediately before the render/capture pin-instantiation path.

## `TakeExclusiveControl` policy flag

The per-stream wrapper constructor at VA `0x40DA54` initializes an internal DWORD at object offset `+0x200` to `1`, then reads:

`HKCU\\Software\\Creative Tech\\CtU2Asio\\TakeExclusiveControl`

The code checks both 64-bit and 32-bit registry views.

Therefore `TakeExclusiveControl` is a real runtime policy flag, not only control-panel text.

Default observed in code: enabled (`1`) unless overridden by the registry value.

## Exact pin-busy sequence

Two independent call sites show the same policy.

### Buffer/format capability path

At VA `0x40CF4C` onward:

1. call pin-instance helper `0x4152F0`
2. if not busy, continue
3. if busy and `TakeExclusiveControl != 0`, call exclusive-preflight helper `0x40E500`
4. call pin-instance helper `0x4152F0` again
5. if still busy, do **not** enter pin instantiation
6. only when the second check reports available instances does the code call pin vtable slot `+0x90`

### Actual stream setup path

At VA `0x40E195` onward:

1. call pin-instance helper `0x4152F0`
2. if busy and `TakeExclusiveControl != 0`, call helper `0x40E500`
3. re-run `0x4152F0`
4. if still busy, skip pin vtable slot `+0x90`
5. if available, call pin vtable slot `+0x90` and store the returned instantiated pin handle/object

The pin-instantiation implementation ultimately resolves `ksuser.dll!KsCreatePin` dynamically at VA `0x416999`–`0x4169E5`.

Therefore the instance-count gate is statically upstream of `KsCreatePin`.

## Exclusive-preflight helper `0x40E500`

When the KS pin is busy and `TakeExclusiveControl` is enabled, Creative does not immediately call `KsCreatePin`.

The helper performs Windows endpoint correlation and an exclusive WASAPI attempt.

Binary-confirmed COM identifiers:

- `CLSID_MMDeviceEnumerator`
  - `{BCDE0395-E52F-467C-8E3D-C4579291692E}`
- `IID_IMMDeviceEnumerator`
  - `{A95664D2-9614-4F35-A746-DE8DB63617E6}`
- `IID_IMMEndpoint`
  - `{1BE09788-6894-4089-8586-9A2A6C265AC5}`
- `IID_IDeviceTopology`
  - `{2A07407E-6497-4A18-9787-32F79BD0D98F}`
- `IID_IPart`
  - `{AE2DE0E4-5BCA-4F2D-AA46-5D13F8FDB3A9}`
- `IID_IAudioClient`
  - `{1CB9AD4C-DBFA-4C32-B178-C2F568A703B2}`

Observed sequence:

1. `CoCreateInstance(CLSID_MMDeviceEnumerator, ..., IID_IMMDeviceEnumerator)`
2. enumerate active audio endpoints
3. use `IMMEndpoint` / `IDeviceTopology` / connector information to correlate the Windows endpoint with the KS device/pin
4. obtain the matching active `IMMDevice`
5. `IMMDevice::Activate(IID_IAudioClient, ...)`
6. `IAudioClient::IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, requestedFormat, NULL)`
7. `IAudioClient::GetDevicePeriod(...)`
8. `IAudioClient::Initialize(`
   - `AUDCLNT_SHAREMODE_EXCLUSIVE`,
   - `AUDCLNT_STREAMFLAGS_EVENTCALLBACK` (`0x00040000`),
   - device-period-derived duration/periodicity,
   - requested wave format,
   - no session GUID)
9. release the temporary COM interfaces
10. return to the caller
11. caller rechecks KS pin instance counts before any `KsCreatePin`

Important precision: the temporary `IAudioClient` is released before the caller proceeds. The static trace therefore proves an exclusive preflight/arbitration attempt and mandatory post-attempt KS instance recheck. It does **not** prove that Creative holds a long-lived WASAPI exclusive stream concurrently with the ASIO KS pin.

## Why this matches the ARM64 crash isolation

Hardware A/B already established:

- no other active X4 render stream -> native SDK baseline PASS
- another active X4 render stream -> GREEN SCREEN

The native SDK baseline currently opens the 48 kHz / 16-bit KS render pin directly and does not perform the Creative instance-availability gate.

The prior and newest crash dumps now share the same `WDF_VIOLATION 0x10D / p1=5` / `usbaudio2` stale-pipe recovery fingerprint.

The Creative static path supplies a concrete missing safety differential:

`query KS pin instances`
-> if busy, optional exclusive endpoint arbitration
-> `query KS pin instances again`
-> only instantiate the KS pin when capacity is available

This is consistent with Creative deliberately avoiding a blind second conflicting KS pin lifecycle.

## What is proven vs not proven

### Binary-confirmed

- registry-backed `TakeExclusiveControl` policy
- default internal value `1`
- `KSPROPERTY_PIN_CINSTANCES` and `KSPROPERTY_PIN_GLOBALCINSTANCES` checks
- comparison `CurrentCount >= PossibleCount`
- check occurs before pin instantiation
- when busy and enabled, Creative performs the MMDevice/DeviceTopology/IAudioClient exclusive path
- instance counts are checked again afterward
- if still busy, the code does not proceed through the observed pin-instantiation call site
- pin instantiation ultimately reaches `KsCreatePin`

### Not yet hardware-confirmed on ARM64/X4

- the exact `CINSTANCES` / `GLOBALCINSTANCES` values with idle X4 playback
- the exact values while Windows X4 playback is active
- whether the instance-count gate alone is sufficient to prevent the known ARM64 `usbaudio2` collision
- whether a native ARM64 implementation should reproduce the optional WASAPI exclusive arbitration or simply fail safely while a competing render stream is active

## Safe next experiment

Do **not** run another crash reproducer.

Build a read-only native ARM64 preflight probe that:

1. discovers the X4 `msft_wave` filter
2. opens the filter only
3. queries Render Pin 1 `KSPROPERTY_PIN_CINSTANCES`
4. queries Render Pin 1 `KSPROPERTY_PIN_GLOBALCINSTANCES`
5. logs `PossibleCount` and `CurrentCount`
6. closes the filter
7. never calls `KsCreatePin`
8. never changes KS state
9. never requests a WaveRT buffer
10. never attempts WASAPI exclusive mode in the first experiment

Run it once with no active X4 playback and once while normal Windows playback is active.

If the active-playback run reports exhausted pin capacity while the idle run does not, implement that exact gate ahead of the native ARM64 `KsCreatePin` path and return a clean busy/coexistence error instead of entering the known crash condition.
