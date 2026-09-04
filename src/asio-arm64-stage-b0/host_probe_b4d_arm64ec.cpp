#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>

#include <cstdio>

#include "asio_compat.h"

#if !defined(_M_ARM64EC)
#error Stage B4D host probe must be compiled as ARM64EC.
#endif

int main() {
    std::puts("Sound Blaster X4 ASIO Stage B4D ARM64EC registered-host probe");

    const HRESULT init_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::printf("CoInitializeEx hr=0x%08lX\n", static_cast<unsigned long>(init_hr));
    if (FAILED(init_hr) && init_hr != RPC_E_CHANGED_MODE) {
        std::puts("B4D HOST PROBE RESULT: FAIL (COM initialization)");
        return 2;
    }

    IASIO* driver = nullptr;
    const HRESULT create_hr = CoCreateInstance(
        CLSID_X4_ARM64_ASIO,
        nullptr,
        CLSCTX_INPROC_SERVER,
        CLSID_X4_ARM64_ASIO,
        reinterpret_cast<void**>(&driver));
    std::printf("CoCreateInstance hr=0x%08lX\n", static_cast<unsigned long>(create_hr));

    bool pass = SUCCEEDED(create_hr) && driver != nullptr;
    if (pass) {
        char name[64]{};
        driver->getDriverName(name);
        const long version = driver->getDriverVersion();
        std::printf("driverName=%s\n", name);
        std::printf("driverVersion=%ld\n", version);
        pass = name[0] != '\0' && version == 107;
        driver->Release();
    }

    if (SUCCEEDED(init_hr)) CoUninitialize();

    std::printf("B4D HOST PROBE RESULT: %s\n",
                pass ? "PASS (REGISTRY COM LOAD + IASIO VTABLE)" : "FAIL");
    return pass ? 0 : 3;
}
