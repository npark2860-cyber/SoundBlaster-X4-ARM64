#include <windows.h>
#include <objbase.h>
#include <audioenginebaseapo.h>
#include <audioengineextensionapo.h>

#include <cstdio>

using DllGetClassObjectFn = HRESULT (STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
using DllCanUnloadNowFn = HRESULT (STDAPICALLTYPE*)();

struct ApoClassCase
{
    const wchar_t* name;
    CLSID clsid;
};

static const ApoClassCase kClasses[] =
{
    { L"SFX", { 0x71dab6a1, 0x39f3, 0x423e, { 0x90, 0xa8, 0x03, 0x27, 0x29, 0x85, 0x11, 0x57 } } },
    { L"MFX", { 0xc624d7b2, 0x8333, 0x448e, { 0x85, 0xc8, 0x51, 0xee, 0xfc, 0x20, 0x25, 0xed } } },
    { L"EFX", { 0xec2f4b76, 0x6ae1, 0x4db9, { 0x8f, 0xf6, 0x34, 0x4b, 0x74, 0xcf, 0x96, 0x50 } } },
};

static void PrintHr(const wchar_t* label, HRESULT hr)
{
    std::wprintf(L"  %-42ls 0x%08lX %ls\n",
        label,
        static_cast<unsigned long>(hr),
        SUCCEEDED(hr) ? L"PASS" : L"FAIL");
}

template <typename T>
static bool ProbeInterface(IUnknown* object, const wchar_t* name)
{
    T* value = nullptr;
    const HRESULT hr = object->QueryInterface(__uuidof(T), reinterpret_cast<void**>(&value));
    PrintHr(name, hr);
    if (value != nullptr)
    {
        value->Release();
    }
    return SUCCEEDED(hr);
}

int wmain(int argc, wchar_t** argv)
{
    const wchar_t* dllPath = argc >= 2 ? argv[1] : L"X4ApoArm64.dll";

    std::wprintf(L"X4 APO ARM64 Stage A0 offline COM probe\n");
    std::wprintf(L"DLL: %ls\n\n", dllPath);
    std::wprintf(L"This probe does not register the DLL, touch an audio endpoint, or call APO Initialize.\n\n");

    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(coHr))
    {
        PrintHr(L"CoInitializeEx", coHr);
        return 2;
    }

    HMODULE module = LoadLibraryW(dllPath);
    if (module == nullptr)
    {
        const DWORD error = GetLastError();
        std::wprintf(L"LoadLibraryW failed: Win32 error %lu (0x%08lX)\n", error, error);
        CoUninitialize();
        return 3;
    }

    auto getClassObject = reinterpret_cast<DllGetClassObjectFn>(
        GetProcAddress(module, "DllGetClassObject"));
    auto canUnloadNow = reinterpret_cast<DllCanUnloadNowFn>(
        GetProcAddress(module, "DllCanUnloadNow"));

    if (getClassObject == nullptr || canUnloadNow == nullptr)
    {
        std::wprintf(L"Required COM exports are missing.\n");
        FreeLibrary(module);
        CoUninitialize();
        return 4;
    }

    bool allPassed = true;

    for (const auto& test : kClasses)
    {
        std::wprintf(L"[%ls]\n", test.name);

        IClassFactory* factory = nullptr;
        HRESULT hr = getClassObject(test.clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory));
        PrintHr(L"DllGetClassObject(IClassFactory)", hr);
        if (FAILED(hr) || factory == nullptr)
        {
            allPassed = false;
            std::wprintf(L"\n");
            continue;
        }

        IAudioProcessingObject* apo = nullptr;
        hr = factory->CreateInstance(
            nullptr,
            __uuidof(IAudioProcessingObject),
            reinterpret_cast<void**>(&apo));
        PrintHr(L"IClassFactory::CreateInstance(APO)", hr);

        if (SUCCEEDED(hr) && apo != nullptr)
        {
            allPassed &= ProbeInterface<IAudioProcessingObject>(apo, L"QI IAudioProcessingObject");
            allPassed &= ProbeInterface<IAudioProcessingObjectRT>(apo, L"QI IAudioProcessingObjectRT");
            allPassed &= ProbeInterface<IAudioProcessingObjectConfiguration>(apo, L"QI IAudioProcessingObjectConfiguration");
            allPassed &= ProbeInterface<IAudioSystemEffects>(apo, L"QI IAudioSystemEffects");
            allPassed &= ProbeInterface<IAudioSystemEffects2>(apo, L"QI IAudioSystemEffects2");
            allPassed &= ProbeInterface<IAudioSystemEffects3>(apo, L"QI IAudioSystemEffects3");
            allPassed &= ProbeInterface<IAudioProcessingObjectNotifications>(apo, L"QI IAudioProcessingObjectNotifications");
            apo->Release();
        }
        else
        {
            allPassed = false;
        }

        factory->Release();
        std::wprintf(L"\n");
    }

    const HRESULT unloadHr = canUnloadNow();
    PrintHr(L"DllCanUnloadNow after releases", unloadHr);
    if (unloadHr != S_OK)
    {
        allPassed = false;
    }

    FreeLibrary(module);
    CoUninitialize();

    std::wprintf(L"\nRESULT: %ls\n", allPassed ? L"PASS" : L"FAIL");
    return allPassed ? 0 : 1;
}
