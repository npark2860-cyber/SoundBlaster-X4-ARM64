# Sound Blaster X4 ARM64 Read-Only Capability Probe

This package contains ARM64 diagnostics for SB1815/X4 control analysis.

## Confirmed runtime prerequisite

Hardware testing on 2026-09-04 established a reproducible conflict condition:

- Creative App running: the independent CTCDC probe can fail to receive the initial `5A 03 00` response.
- Creative App closed: the same probe/session can immediately return the valid maximum-payload response and continue normally.

For independent CTCDC validation, fully close Creative App before running these tools. This is treated as an observed runtime ownership/session-conflict condition; the protocol bytes are not changed to work around it.

## 1. Capability probe

`x4-control-readonly-probe.exe` maps the current SB1815/X4 control state using GET/query operations only.

It sends GET/query operations only for:

- CTCDC session identity and button capability
- Acoustic Engine / playback Malcolm parameters (`0x11`, module `0x96`)
- CrystalVoice / voice-input Malcolm parameters (`0x11`, module `0x95`)
- Graphic EQ query operations (`0x44`)
- Mixer audio-control information (`0x21`)
- Sound Mode active/support queries (`0xA7`)

The executable contains no Direct Mode setter, no Malcolm SET command (`0x12`), no Graphic EQ SET operation, no Sound Mode SET operation, and no raw-command command-line interface.

### Easy run

1. Fully close Creative App.
2. Double-click `RUN-READONLY-CAPABILITY-PROBE.cmd`.
3. Upload `X4_READONLY_CAPABILITY_REPORT.txt`.

The probe auto-detects `USB\\VID_041E&PID_3278&MI_01`; no COM-port argument is normally required.

## 2. CTCDC challenge capture

`x4-ctcdc-challenge-capture.exe` is retained as a diagnostic fallback only if the normal `5A 03 00` readiness query still returns no response while Creative App is confirmed closed.

It reproduces the validated CTCDC COM setup, sends `5A 03 00`, and only if that fails sends the exact CTCDC greeting:

`whoareyou.MyApp8\r\n`

It records the device reply to `X4_CTCDC_CHALLENGE_REPORT.txt` and stops. It does **not** generate/send the cryptographic unlock reply, does not send `SW_MODE1`, and does not send any feature SET command.

Do not change Direct Mode, Malcolm SET, EQ SET, Sound Mode SET, or other X4 feature state while using the read-only capability probe.