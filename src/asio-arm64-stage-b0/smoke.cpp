#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cwchar>

#include "asio_compat.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error Stage B0 smoke test must be built for native Windows ARM64, not ARM64EC.
#endif

using DllGetClassObjectFn = HRESULT (STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
using DllCanUnloadNowFn = HRESULT (STDAPICALLTYPE*)();

int main() {
    std::printf("Sound Blaster X4 ARM64 ASIO Stage B0 COM smoke\n");
    std::printf("SAFETY: no registry writes; no KS open; no KsCreatePin; no WaveRT; no hardware I/O.\n");

    wchar_t exe_path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH)) {
        std::printf("GetModuleFileNameW failed Win32=%lu\n", GetLastError());
        return 2;
    }

    wchar_t* last_slash = wcsrchr(exe_path, L'\\');
    if (!last_slash) {
        std::printf("Executable path has no directory separator\n");
        return 3;
    }
    *(last_slash + 1) = L'\0';

    wchar_t dll_path[MAX_PATH]{};
    wcscpy_s(dll_path, exe_path);
    wcscat_s(dll_path, L"x4-asio-arm64.dll");

    std::wprintf(L"Loading %ls\n", dll_path);
    HMODULE module = LoadLibraryW(dll_path);
    if (!module) {
        std::printf("LoadLibraryW failed Win32=%lu\n", GetLastError());
        return 4;
    }

    auto get_class_object = reinterpret_cast<DllGetClassObjectFn>(
        GetProcAddress(module, "DllGetClassObject"));
    auto can_unload = reinterpret_cast<DllCanUnloadNowFn>(
        GetProcAddress(module, "DllCanUnloadNow"));

    if (!get_class_object || !can_unload) {
        std::printf("Required COM export missing\n");
        FreeLibrary(module);
        return 5;
    }

    IClassFactory* factory = nullptr;
    HRESULT hr = get_class_object(
        CLSID_X4_ARM64_ASIO,
        IID_IClassFactory,
        reinterpret_cast<void**>(&factory));
    std::printf("DllGetClassObject hr=0x%08lX\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !factory) {
        FreeLibrary(module);
        return 6;
    }

    IASIO* driver = nullptr;
    hr = factory->CreateInstance(
        nullptr,
        CLSID_X4_ARM64_ASIO,
        reinterpret_cast<void**>(&driver));
    std::printf("IClassFactory::CreateInstance hr=0x%08lX\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !driver) {
        factory->Release();
        FreeLibrary(module);
        return 7;
    }

    const ASIOBool init_ok = driver->init(nullptr);

    char driver_name[32]{};
    driver->getDriverName(driver_name);

    const long version = driver->getDriverVersion();

    long inputs = -1;
    long outputs = -1;
    const ASIOError channels_hr = driver->getChannels(&inputs, &outputs);

    long min_size = 0;
    long max_size = 0;
    long preferred_size = 0;
    long granularity = 0;
    const ASIOError buffer_hr = driver->getBufferSize(
        &min_size,
        &max_size,
        &preferred_size,
        &granularity);

    ASIOSampleRate sample_rate = 0.0;
    const ASIOError rate_hr = driver->getSampleRate(&sample_rate);
    const ASIOError start_hr = driver->start();

    char error_text[124]{};
    driver->getErrorMessage(error_text);

    std::printf("init=%ld\n", init_ok);
    std::printf("driverName=%s\n", driver_name);
    std::printf("driverVersion=%ld\n", version);
    std::printf("getChannels=%ld inputs=%ld outputs=%ld\n", channels_hr, inputs, outputs);
    std::printf(
        "getBufferSize=%ld min=%ld max=%ld preferred=%ld granularity=%ld\n",
        buffer_hr,
        min_size,
        max_size,
        preferred_size,
        granularity);
    std::printf("getSampleRate=%ld rate=%.1f\n", rate_hr, sample_rate);
    std::printf("start=%ld (Stage B0 expected ASE_InvalidMode=%ld)\n", start_hr, ASE_InvalidMode);
    std::printf("errorMessage=%s\n", error_text);

    driver->Release();
    factory->Release();

    const HRESULT unload_hr = can_unload();
    std::printf("DllCanUnloadNow hr=0x%08lX\n", static_cast<unsigned long>(unload_hr));

    const bool pass =
        init_ok == ASIOTrue &&
        channels_hr == ASE_OK &&
        inputs == 0 &&
        outputs == 2 &&
        buffer_hr == ASE_OK &&
        min_size == 512 &&
        max_size == 512 &&
        preferred_size == 512 &&
        granularity == 0 &&
        rate_hr == ASE_OK &&
        sample_rate == 48000.0 &&
        start_hr == ASE_InvalidMode &&
        unload_hr == S_OK;

    FreeLibrary(module);

    std::printf(pass ? "STAGE B0 COM SMOKE RESULT: PASS\n" : "STAGE B0 COM SMOKE RESULT: FAIL\n");
    return pass ? 0 : 8;
}
