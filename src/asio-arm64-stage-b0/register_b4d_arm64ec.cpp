#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cwchar>

#include "asio_compat.h"

#if !defined(_M_ARM64EC)
#error Stage B4D registration verifier must be compiled as ARM64EC.
#endif

using DllRegisterServerFn = HRESULT (STDAPICALLTYPE*)();
using DllUnregisterServerFn = HRESULT (STDAPICALLTYPE*)();
using DllCanUnloadNowFn = HRESULT (STDAPICALLTYPE*)();

namespace {

bool read_reg_sz(HKEY root, const wchar_t* path, const wchar_t* name,
                 wchar_t* value, DWORD value_count) {
    HKEY key = nullptr;
    const LONG open_result = RegOpenKeyExW(
        root, path, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (open_result != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD bytes = value_count * sizeof(wchar_t);
    const LONG query_result = RegQueryValueExW(
        key, name, nullptr, &type,
        reinterpret_cast<BYTE*>(value), &bytes);
    RegCloseKey(key);
    if (query_result != ERROR_SUCCESS || type != REG_SZ) return false;
    value[value_count - 1] = L'\0';
    return true;
}

bool key_absent(HKEY root, const wchar_t* path) {
    HKEY key = nullptr;
    const LONG result = RegOpenKeyExW(
        root, path, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (result == ERROR_SUCCESS) {
        RegCloseKey(key);
        return false;
    }
    return result == ERROR_FILE_NOT_FOUND;
}

bool get_local_dll_path(wchar_t (&dll_path)[MAX_PATH]) {
    wchar_t exe_path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH)) return false;
    wchar_t* slash = wcsrchr(exe_path, L'\\');
    if (!slash) return false;
    *(slash + 1) = L'\0';
    wcscpy_s(dll_path, exe_path);
    wcscat_s(dll_path, L"x4-asio-arm64ec.dll");
    return true;
}

bool verify_registered(const wchar_t* expected_dll) {
    wchar_t clsid_key[160]{};
    wchar_t inproc_key[192]{};
    wchar_t asio_key[256]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioClsidString);
    swprintf_s(inproc_key, L"%ls\\InprocServer32", clsid_key);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioRegistryName);

    wchar_t clsid_description[256]{};
    wchar_t inproc_path[MAX_PATH]{};
    wchar_t threading_model[64]{};
    wchar_t asio_clsid[128]{};
    wchar_t asio_description[256]{};

    const bool values_ok =
        read_reg_sz(HKEY_LOCAL_MACHINE, clsid_key, nullptr,
                    clsid_description, _countof(clsid_description)) &&
        read_reg_sz(HKEY_LOCAL_MACHINE, inproc_key, nullptr,
                    inproc_path, _countof(inproc_path)) &&
        read_reg_sz(HKEY_LOCAL_MACHINE, inproc_key, L"ThreadingModel",
                    threading_model, _countof(threading_model)) &&
        read_reg_sz(HKEY_LOCAL_MACHINE, asio_key, L"CLSID",
                    asio_clsid, _countof(asio_clsid)) &&
        read_reg_sz(HKEY_LOCAL_MACHINE, asio_key, L"Description",
                    asio_description, _countof(asio_description));

    const bool content_ok = values_ok &&
        _wcsicmp(inproc_path, expected_dll) == 0 &&
        _wcsicmp(threading_model, L"Apartment") == 0 &&
        _wcsicmp(asio_clsid, kX4AsioClsidString) == 0;

    std::wprintf(L"registry CLSID description=%ls\n",
                 values_ok ? clsid_description : L"<missing>");
    std::wprintf(L"registry InprocServer32=%ls\n",
                 values_ok ? inproc_path : L"<missing>");
    std::wprintf(L"registry ThreadingModel=%ls\n",
                 values_ok ? threading_model : L"<missing>");
    std::wprintf(L"registry ASIO CLSID=%ls\n",
                 values_ok ? asio_clsid : L"<missing>");
    std::wprintf(L"registry ASIO Description=%ls\n",
                 values_ok ? asio_description : L"<missing>");
    return content_ok;
}

bool verify_unregistered() {
    wchar_t clsid_key[160]{};
    wchar_t asio_key[256]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioClsidString);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioRegistryName);
    return key_absent(HKEY_LOCAL_MACHINE, clsid_key) &&
           key_absent(HKEY_LOCAL_MACHINE, asio_key);
}

void print_usage() {
    std::puts("Usage: x4-asio-stage-b4d-register.exe --register | --unregister | --verify");
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        print_usage();
        return 2;
    }

    wchar_t dll_path[MAX_PATH]{};
    if (!get_local_dll_path(dll_path)) {
        std::puts("B4D REGISTER RESULT: FAIL (cannot resolve local DLL path)");
        return 3;
    }
    std::wprintf(L"B4D local DLL=%ls\n", dll_path);

    if (_wcsicmp(argv[1], L"--verify") == 0) {
        const bool ok = verify_registered(dll_path);
        std::printf("B4D REGISTER VERIFY RESULT: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 4;
    }

    HMODULE module = LoadLibraryW(dll_path);
    if (!module) {
        std::printf("LoadLibraryW failed Win32=%lu\n", GetLastError());
        std::puts("B4D REGISTER RESULT: FAIL (ARM64EC DLL load)");
        return 5;
    }

    auto register_server = reinterpret_cast<DllRegisterServerFn>(
        GetProcAddress(module, "DllRegisterServer"));
    auto unregister_server = reinterpret_cast<DllUnregisterServerFn>(
        GetProcAddress(module, "DllUnregisterServer"));
    auto can_unload = reinterpret_cast<DllCanUnloadNowFn>(
        GetProcAddress(module, "DllCanUnloadNow"));
    if (!register_server || !unregister_server || !can_unload) {
        std::puts("B4D REGISTER RESULT: FAIL (missing COM exports)");
        FreeLibrary(module);
        return 6;
    }

    int result = 0;
    if (_wcsicmp(argv[1], L"--register") == 0) {
        const HRESULT hr = register_server();
        std::printf("DllRegisterServer hr=0x%08lX\n", static_cast<unsigned long>(hr));
        const bool verified = SUCCEEDED(hr) && verify_registered(dll_path);
        std::printf("B4D REGISTER RESULT: %s\n", verified ? "PASS" : "FAIL");
        result = verified ? 0 : 7;
    } else if (_wcsicmp(argv[1], L"--unregister") == 0) {
        const HRESULT hr = unregister_server();
        std::printf("DllUnregisterServer hr=0x%08lX\n", static_cast<unsigned long>(hr));
        const bool verified = SUCCEEDED(hr) && verify_unregistered();
        std::printf("B4D UNREGISTER RESULT: %s\n", verified ? "PASS" : "FAIL");
        result = verified ? 0 : 8;
    } else {
        print_usage();
        result = 2;
    }

    const HRESULT unload_hr = can_unload();
    std::printf("DllCanUnloadNow hr=0x%08lX\n", static_cast<unsigned long>(unload_hr));
    FreeLibrary(module);
    return result;
}
