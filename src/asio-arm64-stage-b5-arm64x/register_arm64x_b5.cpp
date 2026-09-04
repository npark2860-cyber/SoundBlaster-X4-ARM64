#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cwchar>

#include "../asio-arm64-stage-b0/b5_identity.h"

#if !defined(_M_ARM64EC)
#error ARM64X B5 registration helper must be compiled as ARM64EC.
#endif

namespace {

constexpr wchar_t kForwarderName[] = L"x4-asio-arm64x-b5.dll";
constexpr wchar_t kArm64EcBackendName[] = L"x4_asio_backend_arm64ec_b5.dll";
constexpr wchar_t kArm64BackendName[] = L"x4_asio_backend_arm64_b5.dll";

bool sibling_path(const wchar_t* file_name, wchar_t (&path)[MAX_PATH]) {
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return false;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return false;
    *(slash + 1) = L'\0';
    if (wcscat_s(path, file_name) != 0) return false;
    return true;
}

bool file_exists(const wchar_t* path) {
    const DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

HRESULT set_registry_string(HKEY root, const wchar_t* path,
                            const wchar_t* name, const wchar_t* value) {
    HKEY key = nullptr;
    const LONG open_result = RegCreateKeyExW(
        root, path, 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
    if (open_result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(open_result);

    const DWORD bytes = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    const LONG set_result = RegSetValueExW(
        key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value), bytes);
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(set_result);
}

bool read_registry_string(HKEY root, const wchar_t* path,
                          const wchar_t* name, wchar_t* value,
                          DWORD value_count) {
    HKEY key = nullptr;
    const LONG open_result = RegOpenKeyExW(
        root, path, 0, KEY_READ | KEY_WOW64_64KEY, &key);
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

HRESULT delete_subtree_64(const wchar_t* parent_path, const wchar_t* child_name) {
    HKEY parent = nullptr;
    const LONG open_result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE, parent_path, 0,
        KEY_WRITE | KEY_WOW64_64KEY, &parent);
    if (open_result == ERROR_FILE_NOT_FOUND) return S_OK;
    if (open_result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(open_result);

    const LONG delete_result = RegDeleteTreeW(parent, child_name);
    RegCloseKey(parent);
    if (delete_result == ERROR_SUCCESS || delete_result == ERROR_FILE_NOT_FOUND) {
        return S_OK;
    }
    return HRESULT_FROM_WIN32(delete_result);
}

bool resolve_package_paths(wchar_t (&forwarder)[MAX_PATH],
                           wchar_t (&arm64ec_backend)[MAX_PATH],
                           wchar_t (&arm64_backend)[MAX_PATH]) {
    return sibling_path(kForwarderName, forwarder) &&
           sibling_path(kArm64EcBackendName, arm64ec_backend) &&
           sibling_path(kArm64BackendName, arm64_backend);
}

bool package_files_present(const wchar_t* forwarder,
                           const wchar_t* arm64ec_backend,
                           const wchar_t* arm64_backend) {
    bool ok = true;
    if (!file_exists(forwarder)) {
        std::wprintf(L"missing forwarder: %ls\n", forwarder);
        ok = false;
    }
    if (!file_exists(arm64ec_backend)) {
        std::wprintf(L"missing ARM64EC backend: %ls\n", arm64ec_backend);
        ok = false;
    }
    if (!file_exists(arm64_backend)) {
        std::wprintf(L"missing ARM64 backend: %ls\n", arm64_backend);
        ok = false;
    }
    return ok;
}

HRESULT register_forwarder(const wchar_t* forwarder_path) {
    wchar_t clsid_key[160]{};
    wchar_t inproc_key[192]{};
    wchar_t asio_key[256]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioB5ClsidString);
    swprintf_s(inproc_key, L"%ls\\InprocServer32", clsid_key);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioB5RegistryName);

    HRESULT hr = set_registry_string(
        HKEY_LOCAL_MACHINE, clsid_key, nullptr, kX4AsioB5Description);
    if (FAILED(hr)) return hr;
    hr = set_registry_string(
        HKEY_LOCAL_MACHINE, inproc_key, nullptr, forwarder_path);
    if (FAILED(hr)) return hr;
    hr = set_registry_string(
        HKEY_LOCAL_MACHINE, inproc_key, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;
    hr = set_registry_string(
        HKEY_LOCAL_MACHINE, asio_key, L"CLSID", kX4AsioB5ClsidString);
    if (FAILED(hr)) return hr;
    return set_registry_string(
        HKEY_LOCAL_MACHINE, asio_key, L"Description", kX4AsioB5Description);
}

HRESULT unregister_forwarder() {
    HRESULT hr = delete_subtree_64(
        L"SOFTWARE\\ASIO", kX4AsioB5RegistryName);
    if (FAILED(hr)) return hr;
    return delete_subtree_64(
        L"SOFTWARE\\Classes\\CLSID", kX4AsioB5ClsidString);
}

bool verify_registered(const wchar_t* expected_forwarder) {
    wchar_t clsid_key[160]{};
    wchar_t inproc_key[192]{};
    wchar_t asio_key[256]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioB5ClsidString);
    swprintf_s(inproc_key, L"%ls\\InprocServer32", clsid_key);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioB5RegistryName);

    wchar_t clsid_description[256]{};
    wchar_t inproc_path[MAX_PATH]{};
    wchar_t threading_model[64]{};
    wchar_t asio_clsid[128]{};
    wchar_t asio_description[256]{};

    const bool values_ok =
        read_registry_string(HKEY_LOCAL_MACHINE, clsid_key, nullptr,
                             clsid_description, _countof(clsid_description)) &&
        read_registry_string(HKEY_LOCAL_MACHINE, inproc_key, nullptr,
                             inproc_path, _countof(inproc_path)) &&
        read_registry_string(HKEY_LOCAL_MACHINE, inproc_key, L"ThreadingModel",
                             threading_model, _countof(threading_model)) &&
        read_registry_string(HKEY_LOCAL_MACHINE, asio_key, L"CLSID",
                             asio_clsid, _countof(asio_clsid)) &&
        read_registry_string(HKEY_LOCAL_MACHINE, asio_key, L"Description",
                             asio_description, _countof(asio_description));

    const bool content_ok = values_ok &&
        _wcsicmp(clsid_description, kX4AsioB5Description) == 0 &&
        _wcsicmp(inproc_path, expected_forwarder) == 0 &&
        _wcsicmp(threading_model, L"Apartment") == 0 &&
        _wcsicmp(asio_clsid, kX4AsioB5ClsidString) == 0 &&
        _wcsicmp(asio_description, kX4AsioB5Description) == 0;

    std::wprintf(L"registry InprocServer32=%ls\n",
                 values_ok ? inproc_path : L"<missing>");
    std::wprintf(L"registry ASIO CLSID=%ls\n",
                 values_ok ? asio_clsid : L"<missing>");
    return content_ok;
}

void print_usage() {
    std::puts("Usage: x4-asio-stage-b5-arm64x-register.exe --register | --unregister | --verify");
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        print_usage();
        return 2;
    }

    wchar_t forwarder[MAX_PATH]{};
    wchar_t arm64ec_backend[MAX_PATH]{};
    wchar_t arm64_backend[MAX_PATH]{};
    if (!resolve_package_paths(forwarder, arm64ec_backend, arm64_backend)) {
        std::puts("B5 ARM64X REGISTER RESULT: FAIL (cannot resolve package paths)");
        return 3;
    }

    if (_wcsicmp(argv[1], L"--unregister") == 0) {
        const HRESULT hr = unregister_forwarder();
        std::printf("B5 ARM64X UNREGISTER hr=0x%08lX\n",
                    static_cast<unsigned long>(hr));
        return SUCCEEDED(hr) ? 0 : 8;
    }

    if (!package_files_present(forwarder, arm64ec_backend, arm64_backend)) {
        std::puts("B5 ARM64X REGISTER RESULT: FAIL (package incomplete)");
        return 4;
    }

    if (_wcsicmp(argv[1], L"--verify") == 0) {
        const bool ok = verify_registered(forwarder);
        std::printf("B5 ARM64X REGISTER VERIFY RESULT: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 5;
    }

    if (_wcsicmp(argv[1], L"--register") == 0) {
        const HRESULT hr = register_forwarder(forwarder);
        const bool ok = SUCCEEDED(hr) && verify_registered(forwarder);
        std::printf("B5 ARM64X REGISTER hr=0x%08lX\n",
                    static_cast<unsigned long>(hr));
        std::printf("B5 ARM64X REGISTER RESULT: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 6;
    }

    print_usage();
    return 2;
}
