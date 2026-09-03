#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>

#include "asio_compat.h"
#include "preflight.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error Stage B1 must be built for native Windows ARM64, not ARM64EC.
#endif

namespace {

volatile LONG g_object_count = 0;
volatile LONG g_lock_count = 0;
HMODULE g_module = nullptr;

bool guid_equal(REFGUID a, REFGUID b) {
    return !!InlineIsEqualGUID(a, b);
}

class X4AsioDriver final : public IASIO {
public:
    X4AsioDriver() {
        InterlockedIncrement(&g_object_count);
    }

    ~X4AsioDriver() {
        InterlockedDecrement(&g_object_count);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        if (guid_equal(riid, IID_IUnknown) || guid_equal(riid, CLSID_X4_ARM64_ASIO)) {
            *ppv = static_cast<IASIO*>(this);
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&ref_count_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const LONG count = InterlockedDecrement(&ref_count_);
        if (count == 0) {
            delete this;
            return 0;
        }
        return static_cast<ULONG>(count);
    }

    ASIOBool init(void* sysHandle) override {
        (void)sysHandle;

        const X4InstancePreflightResult preflight = run_x4_instance_preflight();

        if (!preflight.device_found) {
            strcpy_s(last_error_, "Stage B1 preflight FAILED: X4 msft_wave not found");
            return ASIOFalse;
        }

        if (!preflight.filter_opened) {
            sprintf_s(
                last_error_,
                sizeof(last_error_),
                "Stage B1 preflight FAILED: filter open Win32=%lu",
                preflight.open_error);
            return ASIOFalse;
        }

        if (!preflight.local_ok || !preflight.global_ok) {
            sprintf_s(
                last_error_,
                sizeof(last_error_),
                "Stage B1 preflight INDETERMINATE: C ok=%d err=%lu G ok=%d err=%lu",
                preflight.local_ok ? 1 : 0,
                preflight.local_error,
                preflight.global_ok ? 1 : 0,
                preflight.global_error);
            return ASIOFalse;
        }

        const bool local_busy = preflight.local_current >= preflight.local_possible;
        const bool global_busy = preflight.global_current >= preflight.global_possible;

        if (local_busy || global_busy) {
            sprintf_s(
                last_error_,
                sizeof(last_error_),
                "Stage B1 preflight BUSY: C %lu/%lu G %lu/%lu; KsCreatePin SKIPPED",
                preflight.local_current,
                preflight.local_possible,
                preflight.global_current,
                preflight.global_possible);
            return ASIOFalse;
        }

        sprintf_s(
            last_error_,
            sizeof(last_error_),
            "Stage B1 preflight FREE: C %lu/%lu G %lu/%lu; streaming not connected",
            preflight.local_current,
            preflight.local_possible,
            preflight.global_current,
            preflight.global_possible);
        return ASIOTrue;
    }

    void getDriverName(char* name) override {
        if (!name) return;
        strcpy_s(name, 32, kX4AsioDriverName);
    }

    long getDriverVersion() override {
        return 101;
    }

    void getErrorMessage(char* string) override {
        if (!string) return;
        strcpy_s(string, 124, last_error_);
    }

    ASIOError start() override {
        return ASE_InvalidMode;
    }

    ASIOError stop() override {
        return ASE_OK;
    }

    ASIOError getChannels(long* numInputChannels, long* numOutputChannels) override {
        if (!numInputChannels || !numOutputChannels) return ASE_InvalidParameter;
        *numInputChannels = 0;
        *numOutputChannels = 2;
        return ASE_OK;
    }

    ASIOError getLatencies(long* inputLatency, long* outputLatency) override {
        if (!inputLatency || !outputLatency) return ASE_InvalidParameter;
        *inputLatency = 0;
        *outputLatency = 512;
        return ASE_OK;
    }

    ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) override {
        if (!minSize || !maxSize || !preferredSize || !granularity) return ASE_InvalidParameter;
        *minSize = 512;
        *maxSize = 512;
        *preferredSize = 512;
        *granularity = 0;
        return ASE_OK;
    }

    ASIOError canSampleRate(ASIOSampleRate sampleRate) override {
        return sampleRate == 48000.0 ? ASE_OK : ASE_NoClock;
    }

    ASIOError getSampleRate(ASIOSampleRate* sampleRate) override {
        if (!sampleRate) return ASE_InvalidParameter;
        *sampleRate = 48000.0;
        return ASE_OK;
    }

    ASIOError setSampleRate(ASIOSampleRate sampleRate) override {
        return sampleRate == 48000.0 ? ASE_OK : ASE_NoClock;
    }

    ASIOError getClockSources(ASIOClockSource* clocks, long* numSources) override {
        (void)clocks;
        if (!numSources) return ASE_InvalidParameter;
        *numSources = 0;
        return ASE_NotPresent;
    }

    ASIOError setClockSource(long reference) override {
        (void)reference;
        return ASE_NotPresent;
    }

    ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) override {
        (void)sPos;
        (void)tStamp;
        return ASE_NotPresent;
    }

    ASIOError getChannelInfo(ASIOChannelInfo* info) override {
        (void)info;
        return ASE_NotPresent;
    }

    ASIOError createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) override {
        (void)bufferInfos;
        (void)numChannels;
        (void)bufferSize;
        (void)callbacks;
        return ASE_InvalidMode;
    }

    ASIOError disposeBuffers() override {
        return ASE_OK;
    }

    ASIOError controlPanel() override {
        return ASE_NotPresent;
    }

    ASIOError future(long selector, void* opt) override {
        (void)selector;
        (void)opt;
        return ASE_NotPresent;
    }

    ASIOError outputReady() override {
        return ASE_NotPresent;
    }

private:
    volatile LONG ref_count_ = 1;
    char last_error_[124] = "Stage B1 COM preflight not initialized";
};

class X4ClassFactory final : public IClassFactory {
public:
    X4ClassFactory() {
        InterlockedIncrement(&g_object_count);
    }

    ~X4ClassFactory() {
        InterlockedDecrement(&g_object_count);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        if (guid_equal(riid, IID_IUnknown) || guid_equal(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&ref_count_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const LONG count = InterlockedDecrement(&ref_count_);
        if (count == 0) {
            delete this;
            return 0;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;

        auto* object = new (std::nothrow) X4AsioDriver();
        if (!object) return E_OUTOFMEMORY;

        const HRESULT hr = object->QueryInterface(riid, ppv);
        object->Release();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock) {
            InterlockedIncrement(&g_lock_count);
        } else {
            InterlockedDecrement(&g_lock_count);
        }
        return S_OK;
    }

private:
    volatile LONG ref_count_ = 1;
};

HRESULT set_registry_string(HKEY root, const wchar_t* path, const wchar_t* name, const wchar_t* value) {
    HKEY key = nullptr;
    const LONG open_result = RegCreateKeyExW(
        root,
        path,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &key,
        nullptr);
    if (open_result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(open_result);

    const DWORD bytes = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    const LONG set_result = RegSetValueExW(
        key,
        name,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value),
        bytes);
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(set_result);
}

HRESULT register_server() {
    if (!g_module) return E_UNEXPECTED;

    wchar_t module_path[MAX_PATH]{};
    if (!GetModuleFileNameW(g_module, module_path, MAX_PATH)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    wchar_t clsid_key[160]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioClsidString);

    wchar_t inproc_key[192]{};
    swprintf_s(inproc_key, L"%ls\\InprocServer32", clsid_key);

    wchar_t asio_key[256]{};
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioRegistryName);

    HRESULT hr = set_registry_string(HKEY_LOCAL_MACHINE, clsid_key, nullptr, kX4AsioDescription);
    if (FAILED(hr)) return hr;
    hr = set_registry_string(HKEY_LOCAL_MACHINE, inproc_key, nullptr, module_path);
    if (FAILED(hr)) return hr;
    hr = set_registry_string(HKEY_LOCAL_MACHINE, inproc_key, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;
    hr = set_registry_string(HKEY_LOCAL_MACHINE, asio_key, L"CLSID", kX4AsioClsidString);
    if (FAILED(hr)) return hr;
    return set_registry_string(HKEY_LOCAL_MACHINE, asio_key, L"Description", kX4AsioDescription);
}

HRESULT unregister_server() {
    wchar_t clsid_key[160]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioClsidString);

    wchar_t asio_key[256]{};
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioRegistryName);

    const LONG asio_result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, asio_key);
    const LONG clsid_result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsid_key);

    if (asio_result != ERROR_SUCCESS && asio_result != ERROR_FILE_NOT_FOUND) {
        return HRESULT_FROM_WIN32(asio_result);
    }
    if (clsid_result != ERROR_SUCCESS && clsid_result != ERROR_FILE_NOT_FOUND) {
        return HRESULT_FROM_WIN32(clsid_result);
    }
    return S_OK;
}

} // namespace

extern "C" HRESULT __stdcall DllCanUnloadNow() {
    return (g_object_count == 0 && g_lock_count == 0) ? S_OK : S_FALSE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (!guid_equal(rclsid, CLSID_X4_ARM64_ASIO)) return CLASS_E_CLASSNOTAVAILABLE;

    auto* factory = new (std::nothrow) X4ClassFactory();
    if (!factory) return E_OUTOFMEMORY;

    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

extern "C" HRESULT __stdcall DllRegisterServer() {
    return register_server();
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
    return unregister_server();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
