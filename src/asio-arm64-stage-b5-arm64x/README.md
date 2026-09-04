# B5 ARM64X pure-forwarder package

This directory adds a packaging/registration layer only. The validated B5 driver implementations remain separate and unchanged:

- ARM64EC backend: `x4-asio-arm64ec-b5.dll`
- Classic ARM64 backend: `x4-asio-arm64-b5.dll`

The package copies those exact built binaries to loader-safe backend names without modifying their bytes:

- `x4_asio_backend_arm64ec_b5.dll`
- `x4_asio_backend_arm64_b5.dll`

A small Microsoft-style ARM64X pure forwarder is registered as the single COM in-process server:

- `x4-asio-arm64x-b5.dll`

Runtime routing:

- x64 / ARM64EC host -> `x4_asio_backend_arm64ec_b5.dll`
- native ARM64 host -> `x4_asio_backend_arm64_b5.dll`

The forwarder exports only the runtime COM entry points required by hosts:

- `DllGetClassObject`
- `DllCanUnloadNow`

`DllRegisterServer` and `DllUnregisterServer` are intentionally not forwarded, because either backend would otherwise register its own physical DLL path and defeat the ARM64X bridge. Registration is performed by `x4-asio-stage-b5-arm64x-register.exe`, which writes the existing B5 CLSID/ASIO identity with `InprocServer32` pointing to the ARM64X forwarder.

## Safety / scope

This layer does not alter:

- `driver_b5.cpp` streaming behavior
- ARM64EC or Classic ARM64 driver adapters
- WaveRT render/capture code
- mux recovery logic
- BUSY gates
- control panel implementation
- worker teardown ordering

Installing the bridge only updates B5 COM/ASIO registry entries. It does not open or probe WaveRT pins.

## Build

`build_forwarder.cmd` follows Microsoft's ARM64X pure-forwarder link pattern and must run from an ARM64 Visual Studio Developer Command Prompt. The GitHub workflow `build-asio-b5-arm64x-forwarder.yml` performs the complete cross-build and packages the result.

The workflow is manual (`workflow_dispatch`) only.

## Install / verify

From the packaged directory:

1. Run `install_arm64x_b5.cmd`.
2. Run `verify_arm64x_b5.cmd` once on Windows on ARM.

The verifier checks the B5 registry path and then loads the same ARM64X forwarder from both an ARM64EC probe and a native ARM64 probe. Each probe confirms that only the expected backend architecture is loaded.

Use `uninstall_arm64x_b5.cmd` to remove the B5 ARM64X registration.

Do not use `regsvr32` on the ARM64X forwarder; registration is deliberately owned by the dedicated helper so `InprocServer32` cannot be redirected to a backend DLL.
