# DEBUG HISTORY — ASIO Windows SDK ARM64 ABI baseline implemented

Date: 2026-09-03 KST

## Reason for reset

The first native ARM64 A0 test proved that one X4 WaveRT render lifecycle can work, but the implementation manually reconstructed Win32/KS/WaveRT ABI structures and imports. Later same-process repeated lifecycles reproduced `WDF_VIOLATION 0x10D / subtype 0x5`.

Before attributing that failure to the Microsoft USB Audio/WaveRT lifecycle, the client ABI itself must be made authoritative.

## Change in this branch

New implementation path:

`src/asio-sdk-abi-baseline/`

The implementation now consumes Microsoft declarations directly from:

- `windows.h`
- `setupapi.h`
- `ks.h`
- `mmreg.h`
- `ksmedia.h`

No local substitutes for the Win32/KS/WaveRT ABI types are permitted.

## ABI evidence emitted at runtime

The executable logs actual ARM64 compilation values for:

- pointer/HANDLE/GUID size and alignment
- `SP_DEVICE_INTERFACE_DATA`
- `SP_DEVICE_INTERFACE_DETAIL_DATA_W`
- `KSPROPERTY`
- `KSPIN_INTERFACE`
- `KSPIN_MEDIUM`
- `KSPRIORITY`
- `KSPIN_CONNECT`
- `KSDATAFORMAT`
- `WAVEFORMATEX`
- `WAVEFORMATEXTENSIBLE`
- `KSDATAFORMAT_WAVEFORMATEXTENSIBLE`
- `KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION`
- `KSRTAUDIO_BUFFER`
- `KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY`
- `KSAUDIO_PRESENTATION_POSITION`

Critical field offsets are also logged.

A local aggregate contains only the two official SDK types `KSPIN_CONNECT` and `KSDATAFORMAT_WAVEFORMATEXTENSIBLE`; `static_assert` verifies that the format begins exactly at `sizeof(KSPIN_CONNECT)` as required by `KsCreatePin`.

## Hardware test scope

One variable only: ABI implementation source.

Preserved A0 hardware behavior:

- X4 `msft_wave`
- Render Pin 1
- looped streaming interface
- standard medium
- 48 kHz stereo 16-bit PCM
- WAVEFORMATEXTENSIBLE
- requested WaveRT buffer 4096 bytes
- NotificationCount = 2
- entire buffer initialized to silence before RUN
- no buffer writes while RUN is active
- 20 notifications
- packet count + presentation position observation
- clean state unwind and unregister/close
- exactly one lifecycle

Repeated reopen is intentionally excluded.

## ARM64-specific guardrails

- compilation fails unless `_M_ARM64` is defined
- pointer size must be 8 bytes
- build path requires MSVC and the Microsoft Windows SDK
- process/native architecture is logged with `IsWow64Process2`
- `SP_DEVICE_INTERFACE_DETAIL_DATA_W::cbSize` uses the SDK `sizeof`
- if the WaveRT buffer reports `CallMemoryBarrier=TRUE`, the client issues `MemoryBarrier()` after the pre-RUN initialization
- `/Oy-` keeps frame pointers for better ARM64 diagnostics

## Next gate

Build this source as a native ARM64 PE using the manual workflow or MSVC locally, then run exactly once on the X4 and upload:

`x4-asio-sdk-abi-baseline.txt`

Only after this baseline passes should repeated WaveRT lifecycle behavior be investigated again.
