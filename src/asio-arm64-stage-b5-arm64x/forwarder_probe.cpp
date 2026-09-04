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

const char* architecture_name() {
#if defined(_M_ARM64EC)
    return "ARM64EC";
#else
    return "ARM64";
#endif
}

const wchar_t* base_name(const wchar_t* path) {
    const wchar_t* slash = wcsrchr(path, L'\\');
    return slash ? slash + 1 : path;
}

bool module_for_address(const void* address, wchar_t (&path)[MAX_PATH]) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &module)) {
        return false;
    }
    return GetModuleFileNameW(module, path, MAX_PATH) != 0;
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

    HMODULE forwarder = LoadLibraryW(forwarder_path);
    if (!forwarder) {
        std::printf("LoadLibraryW failed Win32=%lu\n", GetLastError());
        std::puts("B5 ARM64X PROBE: FAIL (forwarder load)");
        return 3;
    }

    auto get_class_object = reinterpret_cast<DllGetClassObjectFn>(
        GetProcAddress(forwarder, "DllGetClassObject"));
    auto can_unload = reinterpret_cast<DllCanUnloadNowFn>(
        GetProcAddress(forwarder, "DllCanUnloadNow"));
    if (!get_class_object || !can_unload) {
        std::puts("B5 ARM64X PROBE: FAIL (missing runtime COM exports)");
        FreeLibrary(forwarder);
        return 4;
    }

    wchar_t routed_module[MAX_PATH]{};
    if (!module_for_address(reinterpret_cast<const void*>(get_class_object), routed_module)) {
        std::printf("GetModuleHandleExW failed Win32=%lu\n", GetLastError());
        std::puts("B5 ARM64X PROBE: FAIL (cannot resolve routed backend)");
        FreeLibrary(forwarder);
        return 5;
    }

    IClassFactory* factory = nullptr;
    const HRESULT class_hr = get_class_object(
        CLSID_X4_ARM64_ASIO_B5, IID_IClassFactory,
        reinterpret_cast<void**>(&factory));
    if (FAILED(class_hr) || !factory) {
        std::printf("DllGetClassObject hr=0x%08lX\n",
                    static_cast<unsigned long>(class_hr));
        std::puts("B5 ARM64X PROBE: FAIL (class factory)");
        FreeLibrary(forwarder);
        return 6;
    }
    factory->Release();

    const wchar_t* routed_name = base_name(routed_module);
    std::wprintf(L"routedBackend=%ls\n", routed_name);
    std::wprintf(L"expectedBackend=%ls\n", expected_backend_name());

    const HRESULT unload_hr = can_unload();
    std::printf("DllCanUnloadNow hr=0x%08lX\n",
                static_cast<unsigned long>(unload_hr));

    const bool ok = _wcsicmp(routed_name, expected_backend_name()) == 0;
    std::printf("B5 ARM64X PROBE RESULT: %s\n", ok ? "PASS" : "FAIL");
    FreeLibrary(forwarder);
    return ok ? 0 : 7;
}
