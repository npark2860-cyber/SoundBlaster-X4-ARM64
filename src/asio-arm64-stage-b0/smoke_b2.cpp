#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "asio_compat.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error Stage B2 smoke must be built for native Windows ARM64, not ARM64EC.
#endif

using DllGetClassObjectFn = HRESULT (STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
using DllCanUnloadNowFn = HRESULT (STDAPICALLTYPE*)();

namespace {

bool contains_text(const char* text, const char* needle) {
    return text && needle && std::strstr(text, needle) != nullptr;
}

void read_error(IASIO* driver, char (&buffer)[124]) {
    buffer[0] = '\0';
    driver->getErrorMessage(buffer);
}

} // namespace

int main() {
    std::printf("Sound Blaster X4 ARM64 ASIO Stage B2 fixed WaveRT COM smoke\n");
    std::printf("SAFETY: registry-free; fixed 48k/2ch/16-bit Render Pin 1 only; pre-pin instance gate; no audio writes during RUN.\n");
    std::printf("IMPORTANT: run idle first. Do not bypass a BUSY result.\n");

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
    char init_error[124]{};
    read_error(driver, init_error);
    std::printf("init=%ld\n", init_ok);
    std::printf("initMessage=%s\n", init_error);

    char driver_name[32]{};
    driver->getDriverName(driver_name);
    const long version = driver->getDriverVersion();
    std::printf("driverName=%s\n", driver_name);
    std::printf("driverVersion=%ld\n", version);

    bool pass = false;
    const char* result_label = "FAIL";

    if (init_ok != ASIOTrue) {
        const bool safely_busy =
            contains_text(init_error, "BUSY") &&
            contains_text(init_error, "KsCreatePin SKIPPED");
        std::printf("createBuffers/start/stop=SKIPPED because init did not report FREE\n");
        pass = safely_busy;
        result_label = safely_busy ? "PASS (BUSY SAFELY BLOCKED AT INIT)" : "FAIL";
    } else {
        const ASIOError create_hr = driver->createBuffers(nullptr, 0, 512, nullptr);
        char create_error[124]{};
        read_error(driver, create_error);
        std::printf("createBuffers=%ld\n", create_hr);
        std::printf("createMessage=%s\n", create_error);

        if (create_hr != ASE_OK) {
            const bool safely_busy =
                contains_text(create_error, "PRE-PIN gate BUSY") &&
                contains_text(create_error, "KsCreatePin SKIPPED");
            std::printf("start/stop=SKIPPED because engine preparation did not succeed\n");
            driver->disposeBuffers();
            pass = safely_busy;
            result_label = safely_busy ? "PASS (RACE BUSY SAFELY BLOCKED PRE-PIN)" : "FAIL";
        } else {
            const ASIOError start_hr = driver->start();
            char start_error[124]{};
            read_error(driver, start_error);
            std::printf("start=%ld\n", start_hr);
            std::printf("startMessage=%s\n", start_error);

            const ASIOError stop_hr = driver->stop();
            char stop_error[124]{};
            read_error(driver, stop_error);
            std::printf("stop=%ld\n", stop_hr);
            std::printf("stopMessage=%s\n", stop_error);

            const ASIOError dispose_hr = driver->disposeBuffers();
            char dispose_error[124]{};
            read_error(driver, dispose_error);
            std::printf("disposeBuffers=%ld\n", dispose_hr);
            std::printf("disposeMessage=%s\n", dispose_error);

            pass =
                start_hr == ASE_OK &&
                contains_text(start_error, "20/20 notifications") &&
                contains_text(start_error, "packetDiscontinuities=0") &&
                contains_text(start_error, "positionRegressions=0") &&
                stop_hr == ASE_OK &&
                dispose_hr == ASE_OK;
            result_label = pass ? "PASS (FIXED WAVERT LIFECYCLE)" : "FAIL";
        }
    }

    driver->Release();
    factory->Release();

    const HRESULT unload_hr = can_unload();
    std::printf("DllCanUnloadNow hr=0x%08lX\n", static_cast<unsigned long>(unload_hr));
    pass = pass && unload_hr == S_OK;
    if (unload_hr != S_OK) result_label = "FAIL";

    FreeLibrary(module);

    std::printf("STAGE B2 COM/WAVERT RESULT: %s\n", result_label);
    return pass ? 0 : 8;
}
