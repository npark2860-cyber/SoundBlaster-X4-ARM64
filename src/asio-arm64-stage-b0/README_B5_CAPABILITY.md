# Sound Blaster X4 ASIO B5-0 capability/reference probe

This is the first B5 capability/productization checkpoint. It is intentionally a **measurement-only** batch.

Validated source parent:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

B5 branch:

`exp/windows-arm64-asio-b5-capability-productization`

## Purpose

Collect one capability matrix before changing the proven B4D transport:

1. Creative `SB USB RT ASIO` behavioral contract.
2. Current independent-driver contract.
3. X4 `msft_wave` KS/WaveRT pin data ranges and pin-instance counts.
4. Silent repeated create/start/stop/dispose/reopen behavior.

The output becomes the specification for the next B5 implementation batch covering 24-bit output, confirmed sample rates, selectable buffers, stability, and the narrow stereo input path.

## Safety

The KS probe is property-only. It does **not** call `KsCreatePin`.

`probe_b5.cmd` begins with the same known render-pin instance ownership check used by the validated driver:

- `KSPROPERTY_PIN_CINSTANCES`
- `KSPROPERTY_PIN_GLOBALCINSTANCES`
- known X4 Render Pin 1

If the pin is BUSY or the gate is indeterminate, the script stops before ASIO lifecycle probing.

Do not bypass this gate and do not intentionally recreate the historical concurrent-render collision.

Creative and independent ASIO drivers are instantiated **sequentially**, never concurrently. A second idle-gate check is performed after Creative is fully released and before the independent driver is opened.

## Run

Use the existing working B4D registration for the independent driver. Then, with REAPER and other active X4 playback stopped, run:

```bat
probe_b5.cmd
```

One file is produced:

`B5_CAPABILITY_REPORT.txt`

It contains:

- ASIO registry name / CLSID / registry view
- `getDriverName()` / `getDriverVersion()`
- input/output channel counts
- all channel names and raw ASIO sample types
- buffer min/max/preferred/granularity
- current sample rate
- `canSampleRate()` matrix from 8 kHz through 384 kHz candidates
- clock-source records
- latency values
- three silent lifecycle cycles for Creative
- a post-Creative idle-gate recheck
- three silent lifecycle cycles for the independent driver
- X4 pin count, dataflow, local/global instance counts, and audio data ranges

The lifecycle host zero-fills all output buffers, so it does not intentionally emit program audio.

## Not changed in B5-0

The validated B4D product transport remains untouched:

- 48 kHz
- stereo output
- signed 16-bit PCM
- 512 ASIO frames
- Render Pin 1
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- `writePacket = PacketCount + 1`
- local + global BUSY gates
- joined worker stop
- ASIO 2.x time-info path

Do not implement new transport formats until the capability report is available.
