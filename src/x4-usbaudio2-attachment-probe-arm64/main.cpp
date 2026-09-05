#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <initguid.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

namespace
{
constexpr wchar_t kTargetInstanceNeedle[] = L"USB\\VID_041E&PID_3278&MI_03";

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

std::wstring GuidToString(REFGUID guid)
{
    wchar_t buffer[64]{};
    if (StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer))) == 0)
    {
        return L"{GUID conversion failed}";
    }
    return buffer;
}

const wchar_t* KnownNodeTypeName(REFGUID guid)
{
    if (IsEqualGUID(guid, KSNODETYPE_SPEAKER)) return L"KSNODETYPE_SPEAKER";
    if (IsEqualGUID(guid, KSNODETYPE_HEADPHONES)) return L"KSNODETYPE_HEADPHONES";
    if (IsEqualGUID(guid, KSNODETYPE_MICROPHONE)) return L"KSNODETYPE_MICROPHONE";
    if (IsEqualGUID(guid, KSNODETYPE_SPDIF_INTERFACE)) return L"KSNODETYPE_SPDIF_INTERFACE";
    if (IsEqualGUID(guid, KSNODETYPE_LINE_CONNECTOR)) return L"KSNODETYPE_LINE_CONNECTOR";
    if (IsEqualGUID(guid, KSNODETYPE_DIGITAL_AUDIO_INTERFACE)) return L"KSNODETYPE_DIGITAL_AUDIO_INTERFACE";
    if (IsEqualGUID(guid, KSNODETYPE_ANALOG_CONNECTOR)) return L"KSNODETYPE_ANALOG_CONNECTOR";
    if (IsEqualGUID(guid, KSNODETYPE_ANY)) return L"KSNODETYPE_ANY";
    return L"(unmapped)";
}

std::wstring DeviceIdFromDevInst(DEVINST devInst)
{
    wchar_t id[MAX_DEVICE_ID_LEN]{};
    if (CM_Get_Device_IDW(devInst, id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
    {
        return {};
    }
    return id;
}

bool DevInstBelongsToX4(DEVINST devInst, std::vector<std::wstring>* chainOut = nullptr)
{
    DEVINST current = devInst;
    for (unsigned depth = 0; depth < 16; ++depth)
    {
        std::wstring id = DeviceIdFromDevInst(current);
        if (!id.empty())
        {
            if (chainOut != nullptr)
            {
                chainOut->push_back(id);
            }
            if (ContainsInsensitive(id, kTargetInstanceNeedle))
            {
                return true;
            }
        }

        DEVINST parent = 0;
        if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS)
        {
            break;
        }
        current = parent;
    }
    return false;
}

std::wstring GetSetupDiStringProperty(HDEVINFO set, SP_DEVINFO_DATA& info, DWORD property)
{
    DWORD type = 0;
    DWORD required = 0;
    SetupDiGetDeviceRegistryPropertyW(set, &info, property, &type, nullptr, 0, &required);
    if (required == 0)
    {
        return {};
    }

    std::vector<BYTE> buffer(required + sizeof(wchar_t), 0);
    if (!SetupDiGetDeviceRegistryPropertyW(set, &info, property, &type, buffer.data(), required, nullptr))
    {
        return {};
    }
    return reinterpret_cast<const wchar_t*>(buffer.data());
}

void PrintHexBytes(const BYTE* data, DWORD size)
{
    const DWORD limit = static_cast<DWORD>(64);
    const DWORD shown = size < limit ? size : limit;
    for (DWORD i = 0; i < shown; ++i)
    {
        ::wprintf(L"%02X", data[i]);
        if (i + 1 != shown) ::wprintf(L" ");
    }
    if (shown != size) ::wprintf(L" ... (%lu bytes total)", size);
}

void PrintRegistryValue(DWORD type, const BYTE* data, DWORD size)
{
    switch (type)
    {
    case REG_SZ:
    case REG_EXPAND_SZ:
        if (size >= sizeof(wchar_t))
        {
            ::wprintf(L"\"%ls\"", reinterpret_cast<const wchar_t*>(data));
        }
        break;
    case REG_DWORD:
        if (size >= sizeof(DWORD))
        {
            const DWORD value = *reinterpret_cast<const DWORD*>(data);
            ::wprintf(L"%lu (0x%08lX)", value, value);
        }
        break;
    case REG_QWORD:
        if (size >= sizeof(ULONGLONG))
        {
            const ULONGLONG value = *reinterpret_cast<const ULONGLONG*>(data);
            ::wprintf(L"%llu (0x%016llX)", value, value);
        }
        break;
    case REG_MULTI_SZ:
        if (size >= sizeof(wchar_t))
        {
            const wchar_t* text = reinterpret_cast<const wchar_t*>(data);
            const wchar_t* end = reinterpret_cast<const wchar_t*>(data + size);
            bool first = true;
            while (text < end && *text != L'\0')
            {
                if (!first) ::wprintf(L" | ");
                ::wprintf(L"%ls", text);
                const size_t len = wcslen(text);
                text += len + 1;
                first = false;
            }
        }
        break;
    default:
        PrintHexBytes(data, size);
        break;
    }
}

void DumpRegistryTree(HKEY root, const std::wstring& relativePath, unsigned depth, unsigned indent)
{
    if (depth == 0) return;

    HKEY key = nullptr;
    const LSTATUS openStatus = RegOpenKeyExW(root, relativePath.c_str(), 0, KEY_READ, &key);
    if (openStatus != ERROR_SUCCESS)
    {
        return;
    }

    ::wprintf(L"%*ls[%ls]\n", static_cast<int>(indent), L"", relativePath.c_str());

    DWORD valueIndex = 0;
    for (;; ++valueIndex)
    {
        wchar_t name[512]{};
        DWORD nameChars = static_cast<DWORD>(std::size(name));
        DWORD type = 0;
        DWORD dataSize = 0;
        LSTATUS status = RegEnumValueW(key, valueIndex, name, &nameChars, nullptr, &type, nullptr, &dataSize);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS && status != ERROR_MORE_DATA) continue;

        std::vector<BYTE> data(dataSize + sizeof(wchar_t), 0);
        nameChars = static_cast<DWORD>(std::size(name));
        DWORD actualSize = dataSize;
        status = RegEnumValueW(key, valueIndex, name, &nameChars, nullptr, &type, data.data(), &actualSize);
        if (status != ERROR_SUCCESS) continue;

        ::wprintf(L"%*ls%s = ", static_cast<int>(indent + 2), L"", nameChars == 0 ? L"(Default)" : name);
        PrintRegistryValue(type, data.data(), actualSize);
        ::wprintf(L"  [type=%lu]\n", type);
    }

    DWORD subIndex = 0;
    for (;; ++subIndex)
    {
        wchar_t subName[256]{};
        DWORD subChars = static_cast<DWORD>(std::size(subName));
        FILETIME ft{};
        const LSTATUS status = RegEnumKeyExW(key, subIndex, subName, &subChars, nullptr, nullptr, nullptr, &ft);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS) continue;

        std::wstring child = relativePath;
        child += L"\\";
        child += subName;
        DumpRegistryTree(root, child, depth - 1, indent + 2);
    }

    RegCloseKey(key);
}

void DumpFxEpRoots(HKEY root, const wchar_t* label)
{
    if (root == nullptr || root == INVALID_HANDLE_VALUE) return;
    ::wprintf(L"  Registry view: %ls\n", label);

    HKEY test = nullptr;
    if (RegOpenKeyExW(root, L"FX", 0, KEY_READ, &test) == ERROR_SUCCESS)
    {
        RegCloseKey(test);
        DumpRegistryTree(root, L"FX", 6, 4);
    }
    else
    {
        ::wprintf(L"    FX: (not present)\n");
    }

    if (RegOpenKeyExW(root, L"EP", 0, KEY_READ, &test) == ERROR_SUCCESS)
    {
        RegCloseKey(test);
        DumpRegistryTree(root, L"EP", 6, 4);
    }
    else
    {
        ::wprintf(L"    EP: (not present)\n");
    }
}

void DumpDevNodeRegistry(HDEVINFO set, SP_DEVINFO_DATA& info)
{
    HKEY devKey = SetupDiOpenDevRegKey(set, &info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    if (devKey != INVALID_HANDLE_VALUE)
    {
        DumpFxEpRoots(devKey, L"DIREG_DEV (hardware key)");
        RegCloseKey(devKey);
    }
    else
    {
        ::wprintf(L"  DIREG_DEV open failed: %lu\n", GetLastError());
    }

    HKEY drvKey = SetupDiOpenDevRegKey(set, &info, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
    if (drvKey != INVALID_HANDLE_VALUE)
    {
        DumpFxEpRoots(drvKey, L"DIREG_DRV (software/driver key)");
        RegCloseKey(drvKey);
    }
    else
    {
        ::wprintf(L"  DIREG_DRV open failed: %lu\n", GetLastError());
    }
}

bool KsGetProperty(HANDLE handle, void* request, DWORD requestSize, void* output, DWORD outputSize, DWORD* returned)
{
    DWORD bytes = 0;
    const BOOL ok = DeviceIoControl(
        handle,
        IOCTL_KS_PROPERTY,
        request,
        requestSize,
        output,
        outputSize,
        &bytes,
        nullptr);
    if (returned != nullptr) *returned = bytes;
    return ok != FALSE;
}

void DumpKsPins(const wchar_t* devicePath)
{
    HANDLE handle = CreateFileW(
        devicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        ::wprintf(L"    KS open (read-only) failed: %lu\n", GetLastError());
        return;
    }

    KSPROPERTY countRequest{};
    countRequest.Set = KSPROPSETID_Pin;
    countRequest.Id = KSPROPERTY_PIN_CTYPES;
    countRequest.Flags = KSPROPERTY_TYPE_GET;

    ULONG pinCount = 0;
    DWORD returned = 0;
    if (!KsGetProperty(handle, &countRequest, sizeof(countRequest), &pinCount, sizeof(pinCount), &returned))
    {
        ::wprintf(L"    KSPROPERTY_PIN_CTYPES failed: %lu\n", GetLastError());
        CloseHandle(handle);
        return;
    }

    ::wprintf(L"    KS pin count: %lu\n", pinCount);

    for (ULONG pin = 0; pin < pinCount; ++pin)
    {
        KSP_PIN request{};
        request.Property.Set = KSPROPSETID_Pin;
        request.Property.Flags = KSPROPERTY_TYPE_GET;
        request.PinId = pin;

        GUID category{};
        request.Property.Id = KSPROPERTY_PIN_CATEGORY;
        const bool categoryOk = KsGetProperty(handle, &request, sizeof(request), &category, sizeof(category), &returned);

        KSPIN_DATAFLOW dataFlow = KSPIN_DATAFLOW_IN;
        request.Property.Id = KSPROPERTY_PIN_DATAFLOW;
        const bool flowOk = KsGetProperty(handle, &request, sizeof(request), &dataFlow, sizeof(dataFlow), &returned);

        KSPIN_COMMUNICATION communication = KSPIN_COMMUNICATION_NONE;
        request.Property.Id = KSPROPERTY_PIN_COMMUNICATION;
        const bool commOk = KsGetProperty(handle, &request, sizeof(request), &communication, sizeof(communication), &returned);

        ::wprintf(L"      Pin %lu:\n", pin);
        if (categoryOk)
        {
            ::wprintf(L"        Category: %ls %ls\n", GuidToString(category).c_str(), KnownNodeTypeName(category));
        }
        else
        {
            ::wprintf(L"        Category: query failed (%lu)\n", GetLastError());
        }
        if (flowOk)
        {
            ::wprintf(L"        DataFlow: %ls (%d)\n", dataFlow == KSPIN_DATAFLOW_IN ? L"IN" : L"OUT", static_cast<int>(dataFlow));
        }
        if (commOk)
        {
            ::wprintf(L"        Communication: %d\n", static_cast<int>(communication));
        }
    }

    CloseHandle(handle);
}

void EnumerateX4BaseDevNode()
{
    ::wprintf(L"=== X4 USB audio devnode ===\n");

    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE)
    {
        ::wprintf(L"SetupDiGetClassDevs(all classes) failed: %lu\n\n", GetLastError());
        return;
    }

    bool found = false;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVINFO_DATA info{};
        info.cbSize = sizeof(info);
        if (!SetupDiEnumDeviceInfo(set, index, &info))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }

        const std::wstring id = DeviceIdFromDevInst(info.DevInst);
        if (!ContainsInsensitive(id, kTargetInstanceNeedle)) continue;

        found = true;
        ::wprintf(L"InstanceId: %ls\n", id.c_str());
        ::wprintf(L"Class:      %ls\n", GetSetupDiStringProperty(set, info, SPDRP_CLASS).c_str());
        ::wprintf(L"Service:    %ls\n", GetSetupDiStringProperty(set, info, SPDRP_SERVICE).c_str());
        ::wprintf(L"DriverKey:  %ls\n", GetSetupDiStringProperty(set, info, SPDRP_DRIVER).c_str());
        ::wprintf(L"Friendly:   %ls\n", GetSetupDiStringProperty(set, info, SPDRP_FRIENDLYNAME).c_str());
        DumpDevNodeRegistry(set, info);
        ::wprintf(L"\n");
    }

    if (!found)
    {
        ::wprintf(L"No present devnode containing %ls was found.\n\n", kTargetInstanceNeedle);
    }

    SetupDiDestroyDeviceInfoList(set);
}

void EnumerateCategoryInterfaces(const GUID& category, const wchar_t* categoryName)
{
    ::wprintf(L"=== %ls interfaces belonging to X4 MI_03 ancestry ===\n", categoryName);

    HDEVINFO set = SetupDiGetClassDevsW(&category, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE)
    {
        ::wprintf(L"SetupDiGetClassDevs(interface) failed: %lu\n\n", GetLastError());
        return;
    }

    bool found = false;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &category, index, &iface))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &required, nullptr);
        if (required == 0) continue;

        std::vector<BYTE> storage(required, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA info{};
        info.cbSize = sizeof(info);

        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, required, nullptr, &info))
        {
            continue;
        }

        std::vector<std::wstring> chain;
        if (!DevInstBelongsToX4(info.DevInst, &chain)) continue;

        found = true;
        ::wprintf(L"InterfacePath: %ls\n", detail->DevicePath);
        ::wprintf(L"DevInst chain:\n");
        for (const auto& id : chain)
        {
            ::wprintf(L"  -> %ls\n", id.c_str());
        }

        HKEY ifaceKey = SetupDiOpenDeviceInterfaceRegKey(set, &iface, 0, KEY_READ);
        if (ifaceKey != INVALID_HANDLE_VALUE)
        {
            DumpFxEpRoots(ifaceKey, L"device-interface registry key");
            RegCloseKey(ifaceKey);
        }
        else
        {
            ::wprintf(L"  Device-interface registry key open failed: %lu\n", GetLastError());
        }

        if (IsEqualGUID(category, KSCATEGORY_AUDIO))
        {
            DumpKsPins(detail->DevicePath);
        }

        ::wprintf(L"\n");
    }

    if (!found)
    {
        ::wprintf(L"No matching interfaces found.\n\n");
    }

    SetupDiDestroyDeviceInfoList(set);
}

std::wstring PropVariantString(const PROPVARIANT& value)
{
    if (value.vt == VT_LPWSTR && value.pwszVal != nullptr) return value.pwszVal;
    if (value.vt == VT_BSTR && value.bstrVal != nullptr) return value.bstrVal;
    return {};
}

void EnumerateMmEndpoints()
{
    ::wprintf(L"=== MMDevice endpoints plausibly associated with X4 ===\n");

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr))
    {
        ::wprintf(L"CoCreateInstance(MMDeviceEnumerator) failed: 0x%08lX\n\n", static_cast<unsigned long>(hr));
        return;
    }

    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(eAll, DEVICE_STATEMASK_ALL, &collection);
    if (FAILED(hr))
    {
        ::wprintf(L"EnumAudioEndpoints failed: 0x%08lX\n\n", static_cast<unsigned long>(hr));
        enumerator->Release();
        return;
    }

    UINT count = 0;
    collection->GetCount(&count);
    bool found = false;

    for (UINT i = 0; i < count; ++i)
    {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device)) || device == nullptr) continue;

        IPropertyStore* store = nullptr;
        std::wstring friendly;
        std::wstring instance;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)) && store != nullptr)
        {
            PROPVARIANT value{};
            PropVariantInit(&value);
            if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)))
            {
                friendly = PropVariantString(value);
            }
            PropVariantClear(&value);

            PropVariantInit(&value);
            if (SUCCEEDED(store->GetValue(PKEY_Device_InstanceId, &value)))
            {
                instance = PropVariantString(value);
            }
            PropVariantClear(&value);
            store->Release();
        }

        LPWSTR endpointId = nullptr;
        if (SUCCEEDED(device->GetId(&endpointId)) && endpointId != nullptr)
        {
            const bool plausible = ContainsInsensitive(friendly, L"Sound Blaster X4") ||
                                   ContainsInsensitive(instance, kTargetInstanceNeedle) ||
                                   ContainsInsensitive(endpointId, L"VID_041E&PID_3278");
            if (plausible)
            {
                found = true;
                ::wprintf(L"EndpointId: %ls\n", endpointId);
                ::wprintf(L"Friendly:   %ls\n", friendly.c_str());
                ::wprintf(L"Instance:   %ls\n\n", instance.c_str());
            }
            CoTaskMemFree(endpointId);
        }

        device->Release();
    }

    if (!found)
    {
        ::wprintf(L"No X4-like MMDevice endpoint was identified by read-only properties.\n\n");
    }

    collection->Release();
    enumerator->Release();
}
}

int wmain()
{
    ::wprintf(L"Sound Blaster X4 ARM64 usbaudio2 attachment probe\n");
    ::wprintf(L"READ-ONLY: no registry writes, no endpoint writes, no CTCDC commands, no driver install.\n\n");

    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInitialized = SUCCEEDED(coHr);
    if (!comInitialized && coHr != RPC_E_CHANGED_MODE)
    {
        ::wprintf(L"CoInitializeEx failed: 0x%08lX\n", static_cast<unsigned long>(coHr));
        return 2;
    }

    EnumerateX4BaseDevNode();
    EnumerateCategoryInterfaces(KSCATEGORY_AUDIO, L"KSCATEGORY_AUDIO");
    EnumerateCategoryInterfaces(KSCATEGORY_TOPOLOGY, L"KSCATEGORY_TOPOLOGY");
    EnumerateMmEndpoints();

    if (comInitialized) CoUninitialize();

    ::wprintf(L"=== Probe complete ===\n");
    return 0;
}
