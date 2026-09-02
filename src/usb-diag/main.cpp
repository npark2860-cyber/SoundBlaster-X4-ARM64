#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <hidsdi.h>
#include <usbiodef.h>

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "hid.lib")

namespace
{
struct Node
{
    DEVINST devInst{};
    GUID classGuid{};
    std::wstring instanceId;
    std::wstring parentId;
    std::wstring friendlyName;
    std::wstring description;
    std::wstring manufacturer;
    std::wstring service;
    std::wstring className;
    std::wstring driverKey;
    std::wstring hardwareIds;
    std::wstring compatibleIds;
    std::wstring locationInfo;
    std::wstring locationPaths;
    ULONG status{};
    ULONG problem{};
};

std::wstring upper(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towupper(ch)); });
    return value;
}

std::wstring guid_string(GUID const& guid)
{
    wchar_t buffer[64]{};
    if (StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer))) <= 0)
        return L"<invalid-guid>";
    return buffer;
}

std::wstring parse_reg_buffer(std::vector<BYTE> const& buffer, DWORD regType)
{
    if (buffer.empty())
        return {};

    auto const* text = reinterpret_cast<wchar_t const*>(buffer.data());
    if (regType == REG_SZ || regType == REG_EXPAND_SZ)
        return text;

    if (regType == REG_MULTI_SZ)
    {
        std::wstring result;
        auto const* p = text;
        while (*p)
        {
            if (!result.empty())
                result += L" | ";
            std::wstring item = p;
            result += item;
            p += item.size() + 1;
        }
        return result;
    }

    return {};
}

std::wstring get_reg_property(HDEVINFO set, SP_DEVINFO_DATA& info, DWORD property)
{
    DWORD regType = 0;
    DWORD required = 0;
    SetupDiGetDeviceRegistryPropertyW(set, &info, property, &regType, nullptr, 0, &required);
    if (required == 0)
        return {};

    std::vector<BYTE> buffer(required + sizeof(wchar_t) * 2, 0);
    if (!SetupDiGetDeviceRegistryPropertyW(set, &info, property, &regType,
                                           buffer.data(), static_cast<DWORD>(buffer.size()), nullptr))
        return {};

    return parse_reg_buffer(buffer, regType);
}

std::wstring get_instance_id(HDEVINFO set, SP_DEVINFO_DATA& info)
{
    DWORD required = 0;
    SetupDiGetDeviceInstanceIdW(set, &info, nullptr, 0, &required);
    if (required == 0)
        return {};

    std::vector<wchar_t> buffer(required + 1, L'\0');
    if (!SetupDiGetDeviceInstanceIdW(set, &info, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr))
        return {};
    return buffer.data();
}

std::wstring get_parent_id(DEVINST devInst)
{
    DEVINST parent = 0;
    if (CM_Get_Parent(&parent, devInst, 0) != CR_SUCCESS)
        return {};

    ULONG chars = 0;
    if (CM_Get_Device_ID_Size(&chars, parent, 0) != CR_SUCCESS)
        return {};

    std::vector<wchar_t> buffer(chars + 2, L'\0');
    if (CM_Get_Device_IDW(parent, buffer.data(), static_cast<ULONG>(buffer.size()), 0) != CR_SUCCESS)
        return {};
    return buffer.data();
}

std::wstring get_location_paths(DEVINST devInst)
{
    DEVPROPTYPE type = 0;
    ULONG size = 0;
    auto cr = CM_Get_DevNode_PropertyW(devInst, &DEVPKEY_Device_LocationPaths,
                                      &type, nullptr, &size, 0);
    if (cr != CR_BUFFER_SMALL || size == 0)
        return {};

    std::vector<BYTE> buffer(size + sizeof(wchar_t) * 2, 0);
    cr = CM_Get_DevNode_PropertyW(devInst, &DEVPKEY_Device_LocationPaths,
                                 &type, buffer.data(), &size, 0);
    if (cr != CR_SUCCESS)
        return {};

    auto const* p = reinterpret_cast<wchar_t const*>(buffer.data());
    std::wstring result;
    while (*p)
    {
        if (!result.empty())
            result += L" | ";
        std::wstring item = p;
        result += item;
        p += item.size() + 1;
    }
    return result;
}

bool direct_match(Node const& node)
{
    std::wstring haystack = upper(node.instanceId + L"\n" + node.friendlyName + L"\n" +
                                  node.description + L"\n" + node.hardwareIds);
    return haystack.find(L"VID_041E") != std::wstring::npos ||
           haystack.find(L"SOUND BLASTER X4") != std::wstring::npos ||
           haystack.find(L"SB1815") != std::wstring::npos;
}

bool is_relevant(std::wstring const& id,
                 std::map<std::wstring, Node> const& nodes,
                 std::set<std::wstring> const& direct)
{
    std::wstring current = upper(id);
    std::set<std::wstring> visited;

    for (int depth = 0; depth < 16 && !current.empty(); ++depth)
    {
        if (direct.contains(current))
            return true;
        if (!visited.insert(current).second)
            break;

        auto it = nodes.find(current);
        if (it == nodes.end())
            break;
        current = upper(it->second.parentId);
    }
    return false;
}

void line(std::wostream& out, wchar_t const* key, std::wstring const& value)
{
    if (!value.empty())
        out << L"  " << key << L": " << value << L"\n";
}

void print_node(std::wostream& out, Node const& n)
{
    out << L"------------------------------------------------------------\n";
    out << L"InstanceId: " << n.instanceId << L"\n";
    line(out, L"Parent", n.parentId);
    line(out, L"FriendlyName", n.friendlyName);
    line(out, L"Description", n.description);
    line(out, L"Manufacturer", n.manufacturer);
    line(out, L"Class", n.className);
    out << L"  ClassGuid: " << guid_string(n.classGuid) << L"\n";
    line(out, L"Service", n.service);
    line(out, L"DriverKey", n.driverKey);
    line(out, L"HardwareIds", n.hardwareIds);
    line(out, L"CompatibleIds", n.compatibleIds);
    line(out, L"LocationInfo", n.locationInfo);
    line(out, L"LocationPaths", n.locationPaths);
    out << L"  DevNodeStatus: 0x" << std::hex << std::uppercase << n.status
        << L"  Problem: 0x" << n.problem << std::dec << L"\n";
}

void enumerate_interface_class(std::wostream& out,
                               GUID const& interfaceGuid,
                               wchar_t const* label,
                               std::map<std::wstring, Node> const& nodes,
                               std::set<std::wstring> const& direct)
{
    HDEVINFO set = SetupDiGetClassDevsW(&interfaceGuid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE)
        return;

    out << L"\n=== " << label << L" interface paths ===\n";
    bool any = false;

    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &interfaceGuid, index, &iface))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &required, nullptr);
        if (required == 0)
            continue;

        std::vector<BYTE> detailBuffer(required, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA info{};
        info.cbSize = sizeof(info);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, required, nullptr, &info))
            continue;

        auto id = get_instance_id(set, info);
        if (id.empty())
            continue;

        std::wstring idUpper = upper(id);
        bool relevant = is_relevant(id, nodes, direct) ||
                        idUpper.find(L"VID_041E") != std::wstring::npos;
        if (!relevant)
            continue;

        any = true;
        out << L"InstanceId: " << id << L"\n";
        out << L"  Path: " << detail->DevicePath << L"\n";
    }

    if (!any)
        out << L"<none matched>\n";

    SetupDiDestroyDeviceInfoList(set);
}

int run(std::wostream& out)
{
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE)
    {
        out << L"SetupDiGetClassDevsW failed: " << GetLastError() << L"\n";
        return 10;
    }

    std::map<std::wstring, Node> nodes;

    for (DWORD index = 0;; ++index)
    {
        SP_DEVINFO_DATA info{};
        info.cbSize = sizeof(info);
        if (!SetupDiEnumDeviceInfo(set, index, &info))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }

        Node n{};
        n.devInst = info.DevInst;
        n.classGuid = info.ClassGuid;
        n.instanceId = get_instance_id(set, info);
        if (n.instanceId.empty())
            continue;

        n.parentId = get_parent_id(info.DevInst);
        n.friendlyName = get_reg_property(set, info, SPDRP_FRIENDLYNAME);
        n.description = get_reg_property(set, info, SPDRP_DEVICEDESC);
        n.manufacturer = get_reg_property(set, info, SPDRP_MFG);
        n.service = get_reg_property(set, info, SPDRP_SERVICE);
        n.className = get_reg_property(set, info, SPDRP_CLASS);
        n.driverKey = get_reg_property(set, info, SPDRP_DRIVER);
        n.hardwareIds = get_reg_property(set, info, SPDRP_HARDWAREID);
        n.compatibleIds = get_reg_property(set, info, SPDRP_COMPATIBLEIDS);
        n.locationInfo = get_reg_property(set, info, SPDRP_LOCATION_INFORMATION);
        n.locationPaths = get_location_paths(info.DevInst);
        CM_Get_DevNode_Status(&n.status, &n.problem, info.DevInst, 0);

        nodes.emplace(upper(n.instanceId), std::move(n));
    }

    SetupDiDestroyDeviceInfoList(set);

    std::set<std::wstring> direct;
    for (auto const& [key, node] : nodes)
    {
        if (direct_match(node))
            direct.insert(key);
    }

    out << L"Sound Blaster X4 Windows USB local diagnostic\n";
    out << L"Filter: Creative USB VID_041E, Sound Blaster X4, or SB1815 plus descendants\n\n";

    size_t count = 0;
    for (auto const& [key, node] : nodes)
    {
        if (is_relevant(key, nodes, direct))
        {
            print_node(out, node);
            ++count;
        }
    }

    out << L"\nMatched device nodes: " << count << L"\n";

    enumerate_interface_class(out, GUID_DEVINTERFACE_USB_DEVICE, L"USB device", nodes, direct);

    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);
    enumerate_interface_class(out, hidGuid, L"HID", nodes, direct);

    out << L"\nDiagnostic only: no control command was sent to the X4.\n";
    return count == 0 ? 20 : 0;
}

bool save_utf16le(std::wstring const& path, std::wstring const& text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
        return false;

    constexpr uint16_t bom = 0xFEFF;
    file.write(reinterpret_cast<char const*>(&bom), sizeof(bom));
    file.write(reinterpret_cast<char const*>(text.data()),
               static_cast<std::streamsize>(text.size() * sizeof(wchar_t)));
    return file.good();
}
} // namespace

int wmain()
{
    std::wostringstream report;
    int result = run(report);

    std::wstring text = report.str();
    std::wcout << text;

    if (!save_utf16le(L"x4-usb-diag.txt", text))
    {
        std::wcerr << L"\nCannot create x4-usb-diag.txt in the current directory.\n";
        return result == 0 ? 2 : result;
    }

    std::wcout << L"\nSaved: x4-usb-diag.txt\n";
    return result;
}
