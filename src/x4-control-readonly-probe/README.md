# Sound Blaster X4 ARM64 Read-Only Capability Probe

This probe maps the current SB1815/X4 control state without changing device settings.

## What it sends

After reproducing the already validated CTCDC serial/session setup, it sends GET/query operations only for:

- CTCDC session identity and button capability
- Acoustic Engine / playback Malcolm parameters (`0x11`, module `0x96`)
- CrystalVoice / voice-input Malcolm parameters (`0x11`, module `0x95`)
- Graphic EQ query operations (`0x44`)
- Mixer audio-control information (`0x21`)
- Sound Mode active/support queries (`0xA7`)

The executable contains no Direct Mode setter, no Malcolm SET command (`0x12`), no Graphic EQ SET operation, no Sound Mode SET operation, and no raw-command command-line interface.

## Run

1. Close Creative App or anything else that currently owns the X4 CDC COM port.
2. Run `x4-control-readonly-probe.exe` normally. It auto-detects `USB\\VID_041E&PID_3278&MI_01`.
3. If needed, pass the port explicitly: `x4-control-readonly-probe.exe COM3`.
4. Upload `X4_READONLY_CAPABILITY_REPORT.txt`.

The probe stops before capability queries if the known Maximum Payload Size and firmware session checks do not validate.
