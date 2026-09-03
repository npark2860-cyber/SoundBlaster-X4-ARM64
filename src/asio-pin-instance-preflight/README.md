# Sound Blaster X4 — ARM64 KS Pin-Instance Preflight

This probe validates the Creative x64 ASIO safety gate recovered from `CtU2As64.DLL` without creating any KS streaming pin.

## Safety scope

This executable only:

1. discovers the X4 `msft_wave` KS filter
2. opens the filter
3. queries Render Pin 1 `KSPROPERTY_PIN_CINSTANCES`
4. queries Render Pin 1 `KSPROPERTY_PIN_GLOBALCINSTANCES`
5. prints `PossibleCount` / `CurrentCount`
6. closes the filter

It does **not**:

- call `KsCreatePin`
- request a WaveRT buffer
- register a notification event
- change KS state
- attempt WASAPI exclusive mode
- write audio data

## Interpretation

The Creative x64 reference checks both instance-count properties before pin instantiation and treats the pin as busy when either reports:

`CurrentCount >= PossibleCount`

This probe prints the same combined decision as:

`Creative-equivalent gate busy=YES|NO`

## Hardware A/B

Run the exact same executable in two states:

### A — idle

No active Windows playback stream on the X4.

Save the console output.

### B — normal Windows playback active

Keep a normal Windows X4 render stream playing, then run this probe.

This is safe by design because the probe never creates a second KS render pin.

Compare only the two instance-count results. Do not run the previous crash-producing SDK baseline while playback is active.

## Build

Use the manual GitHub Actions workflow:

`Build ASIO Pin-Instance Preflight ARM64`

or locally from a Visual Studio 2022 Developer Command Prompt:

```bat
cmake -S src/asio-pin-instance-preflight -B build/asio-pin-instance-preflight -G "Visual Studio 17 2022" -A ARM64
cmake --build build/asio-pin-instance-preflight --config Release --parallel
```

Expected executable:

`x4-asio-pin-instance-preflight.exe`
