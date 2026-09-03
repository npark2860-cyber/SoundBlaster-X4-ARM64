# Sound Blaster X4 ARM64 ASIO — Stage B4B host query contract

Stage B4B starts from the hardware-validated asynchronous Stage B4A source:

`exp/windows-arm64-asio-com-stage-b4a-async-worker@996025332bf17341b584095260c1abec93222d84`

It deliberately preserves the B4A WaveRT engine, worker lifetime, coexistence gates, format, packet mapping, DMA copy and stop/cleanup behavior.

## New variable in B4B

Only minimum host-facing inquiry behavior is added:

- native 64-bit `ASIOSamples`
- native 64-bit `ASIOTimeStamp`
- 4-byte-packed `ASIOClockSource`
- 4-byte-packed `ASIOChannelInfo`
- two output channels:
  - channel 0: `X4 Output L`
  - channel 1: `X4 Output R`
  - group 0
  - `ASIOSTInt16LSB`
  - `isActive` reflects whether ASIO buffers exist
- one internal clock source, index 0
- `setClockSource(0)` succeeds; other references fail with `ASE_InvalidParameter`
- `getSamplePosition()` is a logical ASIO block counter:
  - reset to 0 at `start()`
  - latched immediately before each host callback
  - callback 1 = 0
  - callback 2 = 512
  - then +512 frames per successful callback
- timestamp is a monotonic QPC-derived nanosecond value latched with the same block position
- when streaming is not advancing, `getSamplePosition()` returns `ASE_SPNotAdvancing`

The ASIO time-info callback (`bufferSwitchTimeInfo`) is intentionally **not** negotiated in this stage.

## Frozen streaming path

- X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM / `WAVE_FORMAT_EXTENSIBLE`
- 512-frame ASIO buffers
- 4096-byte mapped WaveRT cyclic buffer
- NotificationCount=2
- PacketCount-derived write-ahead slot
- B4A asynchronous worker
- joined `stop()` before KS teardown
- no callback after `stop()` returns

## Smoke

Run only with normal Windows playback on the X4 idle:

```bat
x4-asio-stage-b4b-smoke.exe
```

The smoke is registry-free. It validates:

1. COM creation
2. FREE/BUSY preflight behavior
3. channel metadata before buffer creation (`isActive=0`)
4. one internal clock source
5. `getSamplePosition()` returns `ASE_SPNotAdvancing` before RUN
6. channel metadata becomes active after buffer creation
7. B4A asynchronous `start()` still returns promptly
8. every worker callback observes block-aligned sample positions `0,512,1024,...`
9. timestamps are strictly monotonic
10. normal PCM/DMA transport continues
11. joined stop remains quiescent
12. `getSamplePosition()` returns `ASE_SPNotAdvancing` after stop
13. channels become inactive after dispose
14. COM unload succeeds

A BUSY result must never be bypassed.

## Not included yet

- registry/DAW loading
- `bufferSwitchTimeInfo`
- host time-info negotiation
- MMCSS/AVRT
- capture
- 24-bit
- multichannel
- additional sample rates or buffer sizes
- repeated reopen stress
- Creative runtime dependency
- custom kernel driver
