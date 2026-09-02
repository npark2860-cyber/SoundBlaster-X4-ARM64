#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

using Microsoft::WRL::ComPtr;

namespace
{
std::wstring guid_string(REFGUID guid)
{
    wchar_t buf[64]{};
    if (StringFromGUID2(guid, buf, static_cast<int>(std::size(buf))) <= 0)
        return L"<guid-error>";
    return buf;
}

std::wstring take_cotaskmem_string(LPWSTR value)
{
    std::wstring result = value ? value : L"";
    if (value)
        CoTaskMemFree(value);
    return result;
}

std::wstring property_string(IPropertyStore* store, REFPROPERTYKEY key)
{
    if (!store)
        return {};

    PROPVARIANT pv;
    PropVariantInit(&pv);
    std::wstring result;
    if (SUCCEEDED(store->GetValue(key, &pv)))
    {
        if (pv.vt == VT_LPWSTR && pv.pwszVal)
            result = pv.pwszVal;
        else if (pv.vt == VT_BSTR && pv.bstrVal)
            result = pv.bstrVal;
    }
    PropVariantClear(&pv);
    return result;
}

std::wstring indent(int depth)
{
    return std::wstring(static_cast<size_t>(depth) * 2, L' ');
}

const wchar_t* part_type_name(PartType type)
{
    switch (type)
    {
    case Connector: return L"Connector";
    case Subunit: return L"Subunit";
    default: return L"Unknown";
    }
}

const wchar_t* connector_type_name(ConnectorType type)
{
    switch (type)
    {
    case Unknown_Connector: return L"Unknown";
    case Physical_Internal: return L"Physical_Internal";
    case Physical_External: return L"Physical_External";
    case Software_IO: return L"Software_IO";
    case Software_Fixed: return L"Software_Fixed";
    case Network: return L"Network";
    default: return L"Other";
    }
}

const wchar_t* data_flow_name(DataFlow flow)
{
    switch (flow)
    {
    case In: return L"In";
    case Out: return L"Out";
    default: return L"Unknown";
    }
}

const wchar_t* endpoint_flow_name(EDataFlow flow)
{
    switch (flow)
    {
    case eRender: return L"Render";
    case eCapture: return L"Capture";
    case eAll: return L"All";
    default: return L"Unknown";
    }
}

void dump_part(std::wostream& out,
               ComPtr<IPart> const& part,
               int depth,
               std::set<std::wstring>& visited);

void dump_parts_list(std::wostream& out,
                     IPartsList* list,
                     int depth,
                     wchar_t const* label,
                     std::set<std::wstring>& visited)
{
    if (!list)
        return;

    UINT count = 0;
    if (FAILED(list->GetCount(&count)))
        return;

    out << indent(depth) << label << L" count=" << count << L"\n";
    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IPart> child;
        if (SUCCEEDED(list->GetPart(i, &child)) && child)
            dump_part(out, child, depth + 1, visited);
    }
}

void dump_part(std::wostream& out,
               ComPtr<IPart> const& part,
               int depth,
               std::set<std::wstring>& visited)
{
    if (!part)
        return;

    UINT localId = 0;
    part->GetLocalId(&localId);

    LPWSTR rawGlobal = nullptr;
    std::wstring globalId;
    if (SUCCEEDED(part->GetGlobalId(&rawGlobal)))
        globalId = take_cotaskmem_string(rawGlobal);

    std::wstring visitKey = globalId.empty()
        ? (L"local:" + std::to_wstring(localId))
        : globalId;

    out << indent(depth) << L"PART localId=" << localId;
    if (!globalId.empty())
        out << L" globalId=" << globalId;
    out << L"\n";

    if (!visited.insert(visitKey).second)
    {
        out << indent(depth + 1) << L"(already visited)\n";
        return;
    }

    LPWSTR rawName = nullptr;
    if (SUCCEEDED(part->GetName(&rawName)) && rawName)
        out << indent(depth + 1) << L"Name: " << take_cotaskmem_string(rawName) << L"\n";

    PartType partType{};
    if (SUCCEEDED(part->GetPartType(&partType)))
        out << indent(depth + 1) << L"PartType: " << part_type_name(partType) << L"\n";

    GUID subtype{};
    if (SUCCEEDED(part->GetSubType(&subtype)))
        out << indent(depth + 1) << L"SubType: " << guid_string(subtype) << L"\n";

    UINT controlCount = 0;
    if (SUCCEEDED(part->GetControlInterfaceCount(&controlCount)))
    {
        out << indent(depth + 1) << L"ControlInterfaces: " << controlCount << L"\n";
        for (UINT i = 0; i < controlCount; ++i)
        {
            ComPtr<IControlInterface> ci;
            if (FAILED(part->GetControlInterface(i, &ci)) || !ci)
                continue;

            GUID iid{};
            LPWSTR rawCiName = nullptr;
            std::wstring ciName;
            if (SUCCEEDED(ci->GetName(&rawCiName)) && rawCiName)
                ciName = take_cotaskmem_string(rawCiName);
            ci->GetIID(&iid);

            out << indent(depth + 2) << L"[" << i << L"]";
            if (!ciName.empty())
                out << L" Name=" << ciName;
            out << L" IID=" << guid_string(iid) << L"\n";
        }
    }

    ComPtr<IConnector> connector;
    if (SUCCEEDED(part.As(&connector)) && connector)
    {
        ConnectorType type{};
        DataFlow flow{};
        BOOL connected = FALSE;
        if (SUCCEEDED(connector->GetType(&type)))
            out << indent(depth + 1) << L"ConnectorType: " << connector_type_name(type) << L"\n";
        if (SUCCEEDED(connector->GetDataFlow(&flow)))
            out << indent(depth + 1) << L"ConnectorFlow: " << data_flow_name(flow) << L"\n";
        if (SUCCEEDED(connector->IsConnected(&connected)))
            out << indent(depth + 1) << L"IsConnected: " << (connected ? L"yes" : L"no") << L"\n";

        if (connected)
        {
            ComPtr<IConnector> peer;
            if (SUCCEEDED(connector->GetConnectedTo(&peer)) && peer)
            {
                ComPtr<IPart> peerPart;
                if (SUCCEEDED(peer.As(&peerPart)) && peerPart)
                {
                    out << indent(depth + 1) << L"ConnectedTo:\n";
                    dump_part(out, peerPart, depth + 2, visited);
                }
            }
        }
    }

    ComPtr<IPartsList> incoming;
    if (SUCCEEDED(part->EnumPartsIncoming(&incoming)) && incoming)
        dump_parts_list(out, incoming.Get(), depth + 1, L"Incoming", visited);

    ComPtr<IPartsList> outgoing;
    if (SUCCEEDED(part->EnumPartsOutgoing(&outgoing)) && outgoing)
        dump_parts_list(out, outgoing.Get(), depth + 1, L"Outgoing", visited);
}

std::string utf8(std::wstring const& text)
{
    if (text.empty())
        return {};

    int needed = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
        return {};

    std::string result(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), needed, nullptr, nullptr);
    return result;
}

bool contains_x4(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towupper(c));
    });
    return value.find(L"SOUND BLASTER X4") != std::wstring::npos ||
           value.find(L"VID_041E&PID_3278") != std::wstring::npos;
}
}

int wmain()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        std::wcerr << L"CoInitializeEx failed: 0x" << std::hex << hr << L"\n";
        return 1;
    }

    std::wostringstream out;
    out << L"Sound Blaster X4 Windows Audio DeviceTopology diagnostic\n";
    out << L"Target: X4 USB audio endpoints / MI_03 topology\n";
    out << L"Read-only: no control writes are performed.\n\n";

    ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          IID_PPV_ARGS(&enumerator));
    if (FAILED(hr) || !enumerator)
    {
        out << L"MMDeviceEnumerator creation failed: 0x" << std::hex << hr << L"\n";
        CoUninitialize();
        return 2;
    }

    ComPtr<IMMDeviceCollection> devices;
    hr = enumerator->EnumAudioEndpoints(eAll, DEVICE_STATEMASK_ALL, &devices);
    if (FAILED(hr) || !devices)
    {
        out << L"EnumAudioEndpoints failed: 0x" << std::hex << hr << L"\n";
        CoUninitialize();
        return 3;
    }

    UINT count = 0;
    devices->GetCount(&count);
    UINT matched = 0;

    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IMMDevice> device;
        if (FAILED(devices->Item(i, &device)) || !device)
            continue;

        ComPtr<IPropertyStore> store;
        device->OpenPropertyStore(STGM_READ, &store);
        std::wstring friendly = property_string(store.Get(), PKEY_Device_FriendlyName);
        std::wstring instance = property_string(store.Get(), PKEY_Device_InstanceId);

        LPWSTR rawEndpointId = nullptr;
        std::wstring endpointId;
        if (SUCCEEDED(device->GetId(&rawEndpointId)))
            endpointId = take_cotaskmem_string(rawEndpointId);

        if (!contains_x4(friendly) && !contains_x4(instance) && !contains_x4(endpointId))
            continue;

        ++matched;
        out << L"============================================================\n";
        out << L"Endpoint #" << matched << L"\n";
        out << L"FriendlyName: " << friendly << L"\n";
        out << L"EndpointId: " << endpointId << L"\n";
        if (!instance.empty())
            out << L"InstanceId: " << instance << L"\n";

        DWORD state = 0;
        if (SUCCEEDED(device->GetState(&state)))
            out << L"State: 0x" << std::hex << state << std::dec << L"\n";

        ComPtr<IMMEndpoint> endpoint;
        if (SUCCEEDED(device.As(&endpoint)) && endpoint)
        {
            EDataFlow flow{};
            if (SUCCEEDED(endpoint->GetDataFlow(&flow)))
                out << L"EndpointFlow: " << endpoint_flow_name(flow) << L"\n";
        }

        ComPtr<IDeviceTopology> topology;
        hr = device->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, nullptr,
                              reinterpret_cast<void**>(topology.GetAddressOf()));
        if (FAILED(hr) || !topology)
        {
            out << L"IDeviceTopology activation failed: 0x" << std::hex << hr << std::dec << L"\n\n";
            continue;
        }

        LPWSTR rawTopologyId = nullptr;
        if (SUCCEEDED(topology->GetDeviceId(&rawTopologyId)) && rawTopologyId)
            out << L"TopologyDeviceId: " << take_cotaskmem_string(rawTopologyId) << L"\n";

        UINT connectorCount = 0;
        if (FAILED(topology->GetConnectorCount(&connectorCount)))
        {
            out << L"GetConnectorCount failed.\n\n";
            continue;
        }

        out << L"TopologyConnectors: " << connectorCount << L"\n";
        std::set<std::wstring> visited;
        for (UINT c = 0; c < connectorCount; ++c)
        {
            ComPtr<IConnector> connector;
            if (FAILED(topology->GetConnector(c, &connector)) || !connector)
                continue;

            ComPtr<IPart> part;
            if (SUCCEEDED(connector.As(&part)) && part)
            {
                out << L"\nRootConnector[" << c << L"]:\n";
                dump_part(out, part, 1, visited);
            }
        }
        out << L"\n";
    }

    out << L"Matched X4 audio endpoints: " << matched << L"\n";
    if (matched == 0)
        out << L"No MMDevice endpoint containing Sound Blaster X4 / VID_041E&PID_3278 was found.\n";

    std::string bytes = utf8(out.str());
    std::ofstream file("x4-audio-topology.txt", std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();

    std::wcout << out.str();
    std::wcout << L"\nSaved: x4-audio-topology.txt\n";

    CoUninitialize();
    return 0;
}
