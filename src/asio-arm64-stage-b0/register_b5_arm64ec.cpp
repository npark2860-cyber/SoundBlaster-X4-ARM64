#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cwchar>

#include "b5_identity.h"

#if !defined(_M_ARM64EC)
#error B5 registration verifier must be compiled as ARM64EC.
#endif

using DllRegisterServerFn = HRESULT (STDAPICALLTYPE*)();
using DllUnregisterServerFn = HRESULT (STDAPICALLTYPE*)();

namespace {

bool read_reg_sz(HKEY root, const wchar_t* path, const wchar_t* name,
                 wchar_t* value, DWORD value_count) {
    HKEY key = nullptr;
    const LONG open_result = RegOpenKeyExW(root, path, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (open_result != ERROR_SUCCESS) return false;
    DWORD type = 0;
    DWORD bytes = value_count * sizeof(wchar_t);
    const LONG query_result = RegQueryValueExW(
        key, name, nullptr, &type, reinterpret_cast<BYTE*>(value), &bytes);
    RegCloseKey(key);
    if (query_result != ERROR_SUCCESS || type != REG_SZ) return false;
    value[value_count - 1] = L'\0';
    return true;
}

bool get_local_dll_path(wchar_t (&dll_path)[MAX_PATH]) {
    wchar_t exe_path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH)) return false;
    wchar_t* slash = wcsrchr(exe_path, L'\\');
    if (!slash) return false;
    *(slash + 1) = L'\0';
    wcscpy_s(dll_path, exe_path);
    wcscat_s(dll_path, L"x4-asio-arm64ec-b5.dll");
    return true;
}

bool verify_registered(const wchar_t* expected_dll) {
    wchar_t clsid_key[160]{};
    wchar_t inproc_key[192]{};
    wchar_t asio_key[256]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioB5ClsidString);
    swprintf_s(inproc_key, L"%ls\\InprocServer32", clsid_key);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioB5RegistryName);

    wchar_t inproc_path[MAX_PATH]{};
    wchar_t threading[64]{};
    wchar_t asio_clsid[128]{};
    wchar_t description[256]{};
    const bool ok =
        read_reg_sz(HKEY_LOCAL_MACHINE, inproc_key, nullptr, inproc_path, _countof(inproc_path)) &&
        read_reg_sz(HKEY_LOCAL_MACHINE, inproc_key, L"ThreadingModel", threading, _countof(threading)) &&
        read_reg_sz(HKEY_LOCAL_MACHINE, asio_key, L"CLSID", asio_clsid, _countof(asio_clsid)) &&
        read_reg_sz(HKEY_LOCAL_MACHINE, asio_key, L"Description", description, _countof(description)) &&
        _wcsicmp(inproc_path, expected_dll) == 0 &&
        _wcsicmp(threading, L"Apartment") == 0 &&
        _wcsicmp(asio_clsid, kX4AsioB5ClsidString) == 0;

    std::wprintf(L"B5 registry DLL=%ls\n", inproc_path[0] ? inproc_path : L"<missing>");
    std::wprintf(L"B5 registry ASIO=%ls CLSID=%ls\n",
                 description[0] ? description : L"<missing>",
                 asio_clsid[0] ? asio_clsid : L"<missing>");
    return ok;
}

bool verify_absent() {
    wchar_t clsid_key[160]{};
    wchar_t asio_key[256]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioB5ClsidString);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioB5RegistryName);
    HKEY key = nullptr;
    LONG a = RegOpenKeyExW(HKEY_LOCAL_MACHINE, clsid_key, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (a == ERROR_SUCCESS) { RegCloseKey(key); return false; }
    LONG b = RegOpenKeyExW(HKEY_LOCAL_MACHINE, asio_key, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (b == ERROR_SUCCESS) { RegCloseKey(key); return false; }
    return a == ERROR_FILE_NOT_FOUND && b == ERROR_FILE_NOT_FOUND;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::puts("Usage: x4-asio-stage-b5-register.exe --register | --unregister | --verify");
        return 2;
    }

    wchar_t dll_path[MAX_PATH]{};
    if (!get_local_dll_path(dll_path)) return 3;

    if (_wcsicmp(argv[1], L"--verify") == 0) {
        const bool ok = verify_registered(dll_path);
        std::printf("B5 REGISTER VERIFY RESULT: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 4;
    }

    HMODULE module = LoadLibraryW(dll_path);
    if (!module) {
        std::printf("B5 LoadLibraryW failed Win32=%lu\n", GetLastError());
        return 5;
    }
    auto reg = reinterpret_cast<DllRegisterServerFn>(GetProcAddress(module, "DllRegisterServer"));
    auto unreg = reinterpret_cast<DllUnregisterServerFn>(GetProcAddress(module, "DllUnregisterServer"));
    if (!reg || !unreg) {
        FreeLibrary(module);
        return 6;
    }

    int result = 0;
    if (_wcsicmp(argv[1], L"--register") == 0) {
        const HRESULT hr = reg();
        const bool ok = SUCCEEDED(hr) && verify_registered(dll_path);
        std::printf("B5 REGISTER RESULT: %s hr=0x%08lX\n", ok ? "PASS" : "FAIL", static_cast<unsigned long>(hr));
        result = ok ? 0 : 7;
    } else if (_wcsicmp(argv[1], L"--unregister") == 0) {
        const HRESULT hr = unreg();
        const bool ok = SUCCEEDED(hr) && verify_absent();
        std::printf("B5 UNREGISTER RESULT: %s hr=0x%08lX\n", ok ? "PASS" : "FAIL", static_cast<unsigned long>(hr));
        result = ok ? 0 : 8;
    } else {
        result = 2;
    }

    FreeLibrary(module);
    return result;
}
