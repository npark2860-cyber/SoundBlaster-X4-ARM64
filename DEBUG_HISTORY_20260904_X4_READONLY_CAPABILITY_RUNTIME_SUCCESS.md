# DEBUG HISTORY — X4 Read-Only Capability Runtime Success (2026-09-04)

## Scope

This document records the first complete SB1815/X4 read-only capability-map runtime on the native Windows ARM64 CTCDC path.

No ASIO source was changed for this work. The runtime probe contains no Direct Mode setter, no Malcolm SET (`0x12`), no Graphic EQ SET, no Sound Mode SET, and no raw-command CLI.

## Runtime prerequisite discovered immediately before this run

A repeatable ownership/session conflict was identified:

- Creative App running: the independent CTCDC path can fail at the first `5A 03 00` readiness query with no response.
- Creative App fully closed: the same probe can immediately receive the valid CTCDC response again.

This explains the earlier apparent random `RX=0` behavior without changing any protocol bytes.

For independent controller validation, Creative App must be fully closed before opening the X4 CDC COM session.

## Successful session

Observed on COM3:

- CTCDC serial init: mask `0x05`, 115200/8N1, zero timeouts, purge `0x0F`, `SETDTR`
- `5A 03 00` -> `5A 03 02 3B 00`
- maximum payload = 59 bytes
- firmware query returned `1.9.251008.0930`
- button query returned `5A 26 06 05 00 01 00 1E 00`

The probe then issued all 83 read-only queries without losing the CTCDC session.

Important: `83 / 83` means all 83 queries were issued/completed by the probe loop; it does not mean all 83 received data responses.

## PlaybackManager / Acoustic Engine (`0x11`, module `0x96`)

Request form used:

`5A 11 03 01 96 <parameter>`

### No response

The X4 returned no data response for:

- `0..8`: Surround, Dialog Plus, Smart Volume, Crystalizer
- `23..25`: Bass crossover, Bass enable, Bass strength

Do not infer a writable firmware path for these features from the generic enum alone.

### Confirmed response: Graphic EQ block only

Parameters `9..20` returned normal command-`0x11` frames containing little-endian float32 values.

| Param | Meaning | Runtime value |
|---:|---|---:|
| 9 | Graphic EQ enable | `1.0` |
| 10 | Preamp | `0.0` |
| 11 | band 0 / 31 Hz | `3.0` |
| 12 | band 1 / 62 Hz | `2.0` |
| 13 | band 2 / 125 Hz | `0.0` |
| 14 | band 3 / 250 Hz | `-2.0` |
| 15 | band 4 / 500 Hz | `0.0` |
| 16 | band 5 / 1 kHz | `1.0` |
| 17 | band 6 / 2 kHz | `2.0` |
| 18 | band 7 / 4 kHz | `3.0` |
| 19 | band 8 / 8 kHz | `3.0` |
| 20 | band 9 / 16 kHz | `3.5` |

These ten band values plus preamp and enabled state exactly match the supplied SB1815 `Product/SB1815/Presets/EQ/Music.json` factory/default Music preset.

This is strong runtime confirmation that SB1815/X4 Graphic EQ state is readable through the PlaybackManager Malcolm block.

## VoiceInputManager / CrystalVoice (`0x11`, module `0x95`)

Every queried parameter `0..45` returned no response.

This includes the statically recovered AEC, Noise Reduction, Voice Focus, Voice FX, Mic EQ, and Mic Smart Volume parameter IDs.

The generic enum therefore must not be treated as proof that SB1815 exposes these controls through this raw Malcolm route.

## Direct Graphic Equalizer command (`0x44`)

Read-only operations tested:

- `0x02` get state
- `0x03` get total bands
- `0x04` get range
- `0x06` get all bands
- `0x0A` get support
- `0x0B` get prestored count
- `0x0E` get active index

Every operation returned:

`5A 02 0A 44 81 00 00 00 00 00 00 00 00`

Static re-check of the supplied `Creative.Platform.Devices.dll` recovered `ACKStatusCode`:

- `0x00` GeneralSuccess
- `0x01` DataPending
- `0x02` ConditionalSuccess
- `0x80` GeneralFailure
- `0x81` NotSupported
- `0x82` TemporarilyUnsupported
- `0x83` InvalidParameter
- `0x84` InvalidLength
- `0x85` DeviceBootingUp

Therefore SB1815 explicitly reports **NotSupported** for these direct `0x44` operations in this session.

Do not continue using `0x44` as the X4 EQ backend while the PlaybackManager GEQ block is the confirmed live route.

## Sound Mode command (`0xA7`)

Both read-only operations tested:

- `5A A7 01 01` — GetActiveSoundMode
- `5A A7 01 02` — GetSoundModeSupport

returned the same ACK form with status `0x81`:

`5A 02 0A A7 81 00 00 00 00 00 00 00 00`

Because `0x81` is statically confirmed as `NotSupported`, direct raw `0xA7` is not a valid SB1815 Sound Mode read path in the tested session.

The supplied SB1815 SoundMode JSONs remain useful profile/orchestration data, but the presence of the generic `SoundModeControl` enum must not be treated as X4 hardware support.

## Mixer / AudioControl (`0x21`) — confirmed live firmware route

`5A 21 00` returned a real command-`0x21` response:

`5A 21 1E 0B 0B 00 00 C1 89 C2 89 CE 29 CF 29 D0 29 C3 2A C4 2A C5 2A D1 29 C9 2A BF 00 01 00 00 00`

Static re-check of `RawResAudioControlInformationGet` shows the leading fields:

- `Total` : byte
- `Count` : byte
- `AdditionalPacket` : byte
- `AudioControlIndex` : byte
- `AudioControlInfos` : packed `UInt16[]`

Observed header:

- Total = 11
- Count = 11
- AdditionalPacket = 0
- AudioControlIndex = 0

The official `AudioControl(UInt16)` constructor decodes each descriptor with:

- type = bits `0..5` (`0x003F`)
- HasVolume = `0x0040`
- HasMute = `0x0080`
- IsSource = `0x0100`
- IsRecording = `0x0200`
- HasCustomName = `0x0400`
- IsdBLevel = `0x0800`
- high nibble = channel-count field in the static model

The first 11 descriptors decode as:

| Order / candidate index | Raw | Type | Vol | Mute | Source | Recording | dB | Channel nibble |
|---:|---:|---|:---:|:---:|:---:|:---:|:---:|---:|
| 0 | `0x89C1` | Speaker | Y | Y | Y | N | Y | 8 |
| 1 | `0x89C2` | Headphone | Y | Y | Y | N | Y | 8 |
| 2 | `0x29CE` | SPDIF Output | Y | Y | Y | N | Y | 2 |
| 3 | `0x29CF` | Mic Monitoring | Y | Y | Y | N | Y | 2 |
| 4 | `0x29D0` | Line Monitoring | Y | Y | Y | N | Y | 2 |
| 5 | `0x2AC3` | Mic Input | Y | Y | N | Y | Y | 2 |
| 6 | `0x2AC4` | Line Input | Y | Y | N | Y | Y | 2 |
| 7 | `0x2AC5` | What U Hear recording | Y | Y | N | Y | Y | 2 |
| 8 | `0x29D1` | SPDIF Monitoring | Y | Y | Y | N | Y | 2 |
| 9 | `0x2AC9` | SPDIF Input | Y | Y | N | Y | Y | 2 |
| 10 | `0x00BF` | Automatic Gain Control | N | Y | N | N | N | 0 |

Because `AudioControlIndex=0` and `Count=11`, the next read-only stage will validate the candidate per-control indices `0..10` by querying range/current level/mute.

## Static request structures recovered for the next read-only mixer stage

`RawCmdAudioLevelRangesGet`:

- `Count` : byte
- `Indices` : fixed 32-byte array
- packed size = 33 (`0x21`) bytes

For the candidate indices `0..10`, the exact request layout is therefore expected to be:

`5A 22 21 0B 00 01 02 03 04 05 06 07 08 09 0A` followed by zero padding to fill the 32-byte `Indices` array.

`RawCmdAudioLevelGet` fields:

- `Operation` : byte
- `AudioControlIndex` : byte

With `DevCommOperation.Get = 1`, a per-index read is:

`5A 23 02 01 <index>`

`RawCmdAudioMuteGet` has the same two-byte shape:

`5A 24 02 01 <index>`

Response models recovered statically:

- Audio level: `AudioControlIndex` byte + current `UInt16`
- Audio mute: `AudioControlIndex` byte + current byte
- Range entry: index byte + max/min/step `UInt16`

## Next action

Build a narrow ARM64 **read-only mixer drill-down probe** that:

1. requires the known CTCDC session gate;
2. repeats `0x21` and validates Total/Count/index;
3. sends one exact `0x22` range query for the reported controls;
4. sends `0x23` GET only for descriptors with HasVolume;
5. sends `0x24` GET only for descriptors with HasMute;
6. records all raw TX/RX and parsed values;
7. contains no `0x12`, no mixer SET, and no arbitrary raw-command CLI.

Also add one read-only `5A 10 00` Malcolm sub-feature-support query before any future Malcolm SET work.

No state-changing mixer or EQ command is authorized by this runtime result alone.
