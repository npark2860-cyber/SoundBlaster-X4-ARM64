#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <unknwn.h>

#include <cstdio>
#include <cwchar>

#include "../asio-arm64-stage-b0/b5_identity.h"

using DllGetClassObjectFn = HRESULT (STDAPICALLTYPE*)(REFCLSID, REFIID, void**);
using DllCanUnloadNowFn = HRESULT (STDAPICALLTYPE*)();

namespace {

constexpr wchar_t kForwarderName[] = L"x4-asio-arm64x-b5.dll";
constexpr wchar_t kArm64EcBackendName[] = L"x4_asio_backend_arm64ec_b5.dll";
constexpr wchar_t kArm64BackendName[] = L"x4_asio_backend_arm64_b5.dll";

bool sibling_path(const wchar_t* file_name, wchar_t (&path)[MAX_PATH]) {
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return false;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return false;
    *(slash + 1) = L'\0';
    return wcscat_s(path, file_name) == 0;
}

const wchar_t* expected_backend_name() {
#if defined(_M_ARM64EC)
    return kArm64EcBackendName;
#elif defined(_M_ARM64)
    return kArm64BackendName;
#else
#error ARM64X probe supports only ARM64EC or native ARM64.
#endif
}

const wchar_t* unexpected_backend_name() {
#if defined(_M_ARM64EC)
    return kArm64BackendName;
#else
    return kArm64EcBackendName;
#endif
}

const char* architecture_name() {
#if defined(_M_ARM64EC)
    return "ARM64EC";
#else
    return "ARM64";
#endif
}

} // namespace

int wmain() {
    wchar_t forwarder_path[MAX_PATH]{};
    if (!sibling_path(kForwarderName, forwarder_path)) {
        std::puts("B5 ARM64X PROBE: FAIL (cannot resolve forwarder path)");
        return 2;
    }

    std::printf("B5 ARM64X PROBE architecture=%s\n", architecture_name());
    std::wprintf(L"forwarder=%ls\n", forwarder_path);

    HMODULE module = LoadLibraryW(forwarder_path);
    if (!module) {
        std::printf("LoadLibraryW failed Win32=%lu\n", GetLastError());
        std::puts("B5 ARM64X PROBE: FAIL (forwarder load)");
        return 3;
    }

    auto get_class_object = reinterpret_cast<DllGetClassObjectFn>(
        GetProcAddress(module, "DllGetClassObject"));
    auto can_unload = reinterpret_cast<DllCanUnloadNowFn>(
        GetProcAddress(module, "DllCanUnloadNow"));
    if (!get_class_object || !can_unload) {
        std::puts("B5 ARM64X PROBE: FAIL (missing runtime COM exports)");
        FreeLibrary(module);
        return 4;
    }

    IClassFactory* factory = nullptr;
    const HRESULT class_hr = get_class_object(
        CLSID_X4_ARM64_ASIO_B5, IID_IClassFactory,
        reinterpret_cast<void**>(&factory));
    if (FAILED(class_hr) || !factory) {
        std::printf("DllGetClassObject hr=0x%08lX\n",
                    static_cast<unsigned long>(class_hr));
        std::puts("B5 ARM64X PROBE: FAIL (class factory)");
        FreeLibrary(module);
        return 5;
    }
    factory->Release();

    const HMODULE expected = GetModuleHandleW(expected_backend_name());
    const HMODULE unexpected = GetModuleHandleW(unexpected_backend_name());
    std::wprintf(L"expectedBackend=%ls loaded=%s\n",
                 expected_backend_name(), expected ? L"YES" : L"NO");
    std::wprintf(L"unexpectedBackend=%ls loaded=%s\n",
                 unexpected_backend_name(), unexpected ? L"YES" : L"NO");

    const HRESULT unload_hr = can_unload();
    std::printf("DllCanUnloadNow hr=0x%08lX\n",
                static_cast<unsigned long>(unload_hr));

    const bool ok = expected != nullptr && unexpected == nullptr;
    std::printf("B5 ARM64X PROBE RESULT: %s\n", ok ? "PASS" : "FAIL");
    FreeLibrary(module);
    return ok ? 0 : 6;
}
