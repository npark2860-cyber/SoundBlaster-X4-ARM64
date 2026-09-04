# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated baseline

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Do not alter B4D unless B5 exposes a concrete regression.

Immutable safety remains:

- never bypass local/global BUSY gates
- never intentionally reproduce the historical active-render collision
- never tear hardware down before the worker is joined

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@9ae7ba97277ef2bfb11bb0dbce42f671ed20b20d`

B5 first-release contract and mux-v2 runtime behavior remain unchanged.

Runtime/build marker:

`dual-event-mux-v2`

---

# Latest build evidence

ARM64EC compile failed a second time at:

```text
cguid.h(33,18): error C2059: syntax error: '__uuidof'
```

while compiling `driver_b5_arm64ec.cpp` with Windows SDK 10.0.26100.0.

The earlier claim that moving `#define private public` below SDK/project headers was sufficient is superseded.

The mux driver translation unit had also introduced `winioctl.h`, `ks.h`, and `ksmedia.h` before ASIO/COM headers. Mux v2 does not require those headers directly.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_CGUID_SECOND_FAILURE_KS_HEADER_ISOLATION.md`

---

# Fix now applied

Removed direct KS headers from:

- ARM64EC B5 COM/ASIO driver adapter
- Classic ARM64 B5 COM/ASIO driver adapter

KS headers remain isolated in WaveRT engine translation units.

No packet logic or safety behavior changed.

---

# Immediate action

Run manual workflow once:

`Build ASIO B5 Productization`

Do **not** run hardware validation until the workflow passes all of:

1. ARM64EC DLL compile/link;
2. B5 helper compile/link;
3. Classic ARM64 DLL compile/link;
4. PE/ARM64X checks;
5. `dual-event-mux-v2` present in both DLLs;
6. ZIP package produced.

If the same `cguid.h::__uuidof` C2059 appears again, the next fix is already defined: make the shared B5 source accept ARM64EC directly and remove the adapter's `_M_ARM64` / `_M_ARM64EC` architecture spoof. Do not try another include-order micro-fix.

After build PASS only:

1. download the new `SoundBlaster-X4-ASIO-B5-Productization.zip`;
2. close other X4 playback/default endpoint ownership as before;
3. run `install_and_validate_b5.cmd` once;
4. return the new `B5_PRODUCT_VALIDATION_REPORT.txt`.
