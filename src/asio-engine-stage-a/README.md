# X4 ASIO Engine Stage A

Independent native Windows ARM64 WaveRT render-engine prototype for Sound Blaster X4.

## Fixed scope

- X4 `msft_wave` filter discovery
- Render Pin 1 only
- 48 kHz
- stereo
- 16-bit PCM
- 4096-byte WaveRT cyclic buffer
- notification count 2
- two logical 512-frame host buffers
- callback index from completed packet count (`0/1/0/1/...`)
- sample position from `KSPROPERTY_RTAUDIO_PRESENTATION_POSITION`
- packet continuity from `KSPROPERTY_RTAUDIO_PACKETCOUNT`
- clean notification unregister / STOP / close
- 3 complete open-run-stop-close cycles, 64 callbacks each

No capture, 24-bit, 96/192 kHz, multichannel, dynamic buffer size, sample-rate switching, ASIO COM registration, or Creative runtime DLLs are included in Stage A.

## Expected runtime log

`x4-asio-engine-stage-a.txt`

PASS requires:

- 192 callbacks total
- packet discontinuities = 0
- logical-buffer alternation errors = 0
- sample-position regressions = 0
- all 3 full resource lifecycles complete

The logical-buffer callback currently fills the completed half-buffer with silence. Stage B will replace that test callback with the actual ASIO host-facing buffer callback layer.
