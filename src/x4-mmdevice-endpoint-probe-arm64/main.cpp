#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <string>

namespace
{
std::wstring ToUpper(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towupper(ch));
    });
    return value;
}

bool ContainsInsensitive(const std::wstring& haystack, const std::wstring& needle)
{
    return ToUpper(haystack).find(ToUpper(needle)) != std::wstring::npos;
}

std::wstring ReadString(IPropertyStore* store, REFPROPERTYKEY key)
{
    PROPVARIANT value{};
    PropVariantInit(&value);
    std::wstring result;
    if (SUCCEEDED(store->GetValue(key, &value)))
    {
        if (value.vt == VT_LPWSTR && value.pwszVal != nullptr)
        {
            result = value.pwszVal;
        }
        else if (value.vt == VT_BSTR && value.bstrVal != nullptr)
        {
            result = value.bstrVal;
        }
    }
    PropVariantClear(&value);
    return result;
}

bool ReadUInt32(IPropertyStore* store, REFPROPERTYKEY key, UINT32* result)
{
    if (result == nullptr) return false;

    PROPVARIANT value{};
    PropVariantInit(&value);
    const HRESULT hr = store->GetValue(key, &value);
    bool ok = false;
    if (SUCCEEDED(hr) && value.vt == VT_UI4)
    {
        *result = value.ulVal;
        ok = true;
    }
    PropVariantClear(&value);
    return ok;
}

const wchar_t* FormFactorName(UINT32 value)
{
    switch (static_cast<EndpointFormFactor>(value))
    {
    case RemoteNetworkDevice: return L"RemoteNetworkDevice";
    case Speakers: return L"Speakers";
    case LineLevel: return L"LineLevel";
    case Headphones: return L"Headphones";
    case Microphone: return L"Microphone";
    case Headset: return L"Headset";
    case Handset: return L"Handset";
    case UnknownDigitalPassthrough: return L"UnknownDigitalPassthrough";
    case SPDIF: return L"SPDIF";
    case DigitalAudioDisplayDevice: return L"DigitalAudioDisplayDevice";
    case UnknownFormFactor: return L"UnknownFormFactor";
    default: return L"(unmapped)";
    }
}

const wchar_t* DataFlowName(EDataFlow flow)
{
    switch (flow)
    {
    case eRender: return L"Render";
    case eCapture: return L"Capture";
    case eAll: return L"All";
    default: return L"Unknown";
    }
}

void PrintEndpoint(IMMDevice* device)
{
    IPropertyStore* store = nullptr;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, &store);
    if (FAILED(hr) || store == nullptr) return;

    const std::wstring friendly = ReadString(store, PKEY_Device_FriendlyName);
    if (!ContainsInsensitive(friendly, L"Sound Blaster X4"))
    {
        store->Release();
        return;
    }

    LPWSTR endpointId = nullptr;
    device->GetId(&endpointId);

    DWORD state = 0;
    device->GetState(&state);

    EDataFlow flow = eAll;
    IMMEndpoint* endpoint = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&endpoint))) && endpoint != nullptr)
    {
        endpoint->GetDataFlow(&flow);
        endpoint->Release();
    }

    const std::wstring association = ReadString(store, PKEY_AudioEndpoint_Association);
    UINT32 formFactor = 0;
    const bool haveFormFactor = ReadUInt32(store, PKEY_AudioEndpoint_FormFactor, &formFactor);

    ::wprintf(L"EndpointId:  %ls\n", endpointId != nullptr ? endpointId : L"(unavailable)");
    ::wprintf(L"Flow:        %ls\n", DataFlowName(flow));
    ::wprintf(L"State:       0x%08lX\n", static_cast<unsigned long>(state));
    ::wprintf(L"Friendly:    %ls\n", friendly.c_str());
    ::wprintf(L"Association: %ls\n", association.empty() ? L"(missing)" : association.c_str());
    if (haveFormFactor)
    {
        ::wprintf(L"FormFactor:  %u (%ls)\n", static_cast<unsigned int>(formFactor), FormFactorName(formFactor));
    }
    else
    {
        ::wprintf(L"FormFactor:  (missing or unexpected PROPVARIANT type)\n");
    }
    ::wprintf(L"\n");

    if (endpointId != nullptr) CoTaskMemFree(endpointId);
    store->Release();
}
}

int wmain()
{
    ::wprintf(L"Sound Blaster X4 ARM64 MMDevice endpoint association probe\n");
    ::wprintf(L"READ-ONLY: no property writes, no registry writes, no CTCDC commands, no driver install.\n\n");

    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(coHr) && coHr != RPC_E_CHANGED_MODE)
    {
        ::wprintf(L"CoInitializeEx failed: 0x%08lX\n", static_cast<unsigned long>(coHr));
        return 2;
    }
    const bool uninitialize = SUCCEEDED(coHr);

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr) || enumerator == nullptr)
    {
        ::wprintf(L"CoCreateInstance(MMDeviceEnumerator) failed: 0x%08lX\n", static_cast<unsigned long>(hr));
        if (uninitialize) CoUninitialize();
        return 3;
    }

    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(eAll, DEVICE_STATEMASK_ALL, &collection);
    if (FAILED(hr) || collection == nullptr)
    {
        ::wprintf(L"EnumAudioEndpoints failed: 0x%08lX\n", static_cast<unsigned long>(hr));
        enumerator->Release();
        if (uninitialize) CoUninitialize();
        return 4;
    }

    UINT count = 0;
    collection->GetCount(&count);
    UINT matched = 0;
    for (UINT index = 0; index < count; ++index)
    {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(index, &device)) || device == nullptr) continue;

        IPropertyStore* store = nullptr;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)) && store != nullptr)
        {
            const std::wstring friendly = ReadString(store, PKEY_Device_FriendlyName);
            if (ContainsInsensitive(friendly, L"Sound Blaster X4")) ++matched;
            store->Release();
        }

        PrintEndpoint(device);
        device->Release();
    }

    ::wprintf(L"Matched X4 endpoints: %u\n", static_cast<unsigned int>(matched));
    ::wprintf(L"=== Probe complete ===\n");

    collection->Release();
    enumerator->Release();
    if (uninitialize) CoUninitialize();
    return matched == 0 ? 5 : 0;
}
