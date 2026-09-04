# DEBUG HISTORY — ASIO B4D REAPER engine first success

Date: 2026-09-04 KST

## Runtime milestone

REAPER Windows ARM build successfully selected the registered `Sound Blaster X4 native ARM64 ASIO` driver and exposed the two expected output channels:

- `1: X4 Output L`
- `2: X4 Output R`

The REAPER audio status bar then showed the ASIO engine active at the frozen first-test configuration:

```text
[48kHz 24bit WAV : 0/2ch 512spls ~10/10ms ASIO]
```

Interpretation:

- REAPER has loaded the registered ARM64EC ASIO DLL.
- REAPER accepted the ASIO device and its two output channels.
- The host audio engine is running at 48 kHz with a 512-sample block.
- The displayed `24bit WAV` describes the media/project-side WAV format shown by REAPER; the current driver transport remains the intentionally frozen stereo signed 16-bit PCM implementation.
- The approximate `~10/10ms ASIO` status is consistent with 512 frames at 48 kHz (~10.67 ms per block).

This follows the already proven B4D registration + normal COM registry load + IASIO vtable probe.

## Significance

This is the first real-DAW proof that the independent Sound Blaster X4 Windows-on-Arm ASIO implementation can be enumerated, selected, and brought to an active ASIO engine state by REAPER.

The next proof remains actual REAPER project PCM audibility through the X4 plus stop/start/close stability.
