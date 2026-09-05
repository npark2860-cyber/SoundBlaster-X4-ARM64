# X4 APO ARM64 Offline COM Probe

This is the Stage A0 offline validation companion for `X4ApoArm64.dll`.

It does **not**:

- register the APO DLL;
- write registry values;
- access or modify a Sound Blaster X4 endpoint;
- call `IAudioProcessingObject::Initialize`;
- issue CTCDC commands;
- change any Creative FX property;
- load AudioDG.

It directly loads the DLL with `LoadLibraryW`, resolves `DllGetClassObject`, and checks the three SB1815/X4 APO CLSIDs:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

For each class it validates:

- `IClassFactory` creation through `DllGetClassObject`;
- `IClassFactory::CreateInstance` for `IAudioProcessingObject`;
- QI for `IAudioProcessingObject`;
- QI for `IAudioProcessingObjectRT`;
- QI for `IAudioProcessingObjectConfiguration`;
- QI for `IAudioSystemEffects`;
- QI for `IAudioSystemEffects2`;
- QI for `IAudioSystemEffects3`;
- QI for `IAudioProcessingObjectNotifications`;
- `DllCanUnloadNow == S_OK` after all objects are released.

## Run

Place the EXE and `X4ApoArm64.dll` in the same directory and run:

```bat
X4ApoComProbeArm64.exe X4ApoArm64.dll
```

Expected final line:

```text
RESULT: PASS
```

A failure at this stage is a COM/loader/class-factory problem only. Do not proceed to APO installation or endpoint binding until this probe passes.
