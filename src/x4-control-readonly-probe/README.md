# Sound Blaster X4 ARM64 Read-Only Capability Probe

This package contains two ARM64 diagnostics for SB1815/X4 control analysis.

## 1. CTCDC challenge capture

Run `x4-ctcdc-challenge-capture.exe` first when the normal `5A 03 00` readiness query returns no response.

It reproduces the validated CTCDC COM setup, sends `5A 03 00`, and only if that fails sends the exact CTCDC greeting:

`whoareyou.MyApp8\r\n`

It records the device reply to `X4_CTCDC_CHALLENGE_REPORT.txt` and stops. It does **not** generate/send the cryptographic unlock reply, does not send `SW_MODE1`, and does not send any feature SET command.

## 2. Capability probe

`x4-control-readonly-probe.exe` maps the current SB1815/X4 control state after a valid CTCDC session is already available.

It sends GET/query operations only for:

- CTCDC session identity and button capability
- Acoustic Engine / playback Malcolm parameters (`0x11`, module `0x96`)
- CrystalVoice / voice-input Malcolm parameters (`0x11`, module `0x95`)
- Graphic EQ query operations (`0x44`)
- Mixer audio-control information (`0x21`)
- Sound Mode active/support queries (`0xA7`)

The executable contains no Direct Mode setter, no Malcolm SET command (`0x12`), no Graphic EQ SET operation, no Sound Mode SET operation, and no raw-command command-line interface.

## Run

1. Close Creative App or anything else that currently owns the X4 CDC COM port.
2. Run `x4-ctcdc-challenge-capture.exe` normally. It auto-detects `USB\\VID_041E&PID_3278&MI_01`.
3. If needed, pass the port explicitly: `x4-ctcdc-challenge-capture.exe COM3`.
4. Upload `X4_CTCDC_CHALLENGE_REPORT.txt`.

Do not run the capability probe again until the current CTCDC session state is understood from the challenge report.
