#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
std::wstring upper(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towupper(c));
    });
    return s;
}

std::wstring get_instance_id(HDEVINFO set, SP_DEVINFO_DATA& info)
{
    DWORD needed = 0;
    SetupDiGetDeviceInstanceIdW(set, &info, nullptr, 0, &needed);
    if (!needed)
        return {};
    std::vector<wchar_t> buf(needed + 1, L'\0');
    if (!SetupDiGetDeviceInstanceIdW(set, &info, buf.data(), static_cast<DWORD>(buf.size()), nullptr))
        return {};
    return buf.data();
}

std::wstring get_string(HANDLE h, BOOLEAN(__stdcall* fn)(HANDLE, PVOID, ULONG))
{
    std::array<wchar_t, 256> buf{};
    if (!fn(h, buf.data(), static_cast<ULONG>(buf.size() * sizeof(wchar_t))))
        return {};
    return buf.data();
}

const wchar_t* report_name(HIDP_REPORT_TYPE t)
{
    switch (t)
    {
    case HidP_Input: return L"Input";
    case HidP_Output: return L"Output";
    case HidP_Feature: return L"Feature";
    default: return L"Unknown";
    }
}

void print_button_caps(std::wostream& out,
                       HIDP_REPORT_TYPE type,
                       PHIDP_PREPARSED_DATA ppd,
                       USHORT count)
{
    out << L"\n=== " << report_name(type) << L" Button Caps (" << count << L") ===\n";
    if (!count)
        return;

    std::vector<HIDP_BUTTON_CAPS> caps(count);
    USHORT actual = count;
    auto status = HidP_GetButtonCaps(type, caps.data(), &actual, ppd);
    if (status != HIDP_STATUS_SUCCESS)
    {
        out << L"HidP_GetButtonCaps failed: 0x" << std::hex << std::uppercase
            << static_cast<ULONG>(status) << std::dec << L"\n";
        return;
    }

    for (USHORT i = 0; i < actual; ++i)
    {
        auto const& c = caps[i];
        out << L"[" << i << L"] UsagePage=0x" << std::hex << std::setw(4) << std::setfill(L'0')
            << c.UsagePage << L" ReportID=0x" << std::setw(2) << static_cast<unsigned>(c.ReportID)
            << L" BitField=0x" << std::setw(4) << c.BitField
            << L" LinkCollection=" << std::dec << c.LinkCollection
            << L" IsAlias=" << static_cast<unsigned>(c.IsAlias)
            << L" IsRange=" << static_cast<unsigned>(c.IsRange)
            << L" IsStringRange=" << static_cast<unsigned>(c.IsStringRange)
            << L" IsDesignatorRange=" << static_cast<unsigned>(c.IsDesignatorRange)
            << L" IsAbsolute=" << static_cast<unsigned>(c.IsAbsolute) << L"\n";

        if (c.IsRange)
        {
            out << L"    UsageMin=0x" << std::hex << c.Range.UsageMin
                << L" UsageMax=0x" << c.Range.UsageMax << std::dec << L"\n";
        }
        else
        {
            out << L"    Usage=0x" << std::hex << c.NotRange.Usage << std::dec << L"\n";
        }
    }
}

void print_value_caps(std::wostream& out,
                      HIDP_REPORT_TYPE type,
                      PHIDP_PREPARSED_DATA ppd,
                      USHORT count)
{
    out << L"\n=== " << report_name(type) << L" Value Caps (" << count << L") ===\n";
    if (!count)
        return;

    std::vector<HIDP_VALUE_CAPS> caps(count);
    USHORT actual = count;
    auto status = HidP_GetValueCaps(type, caps.data(), &actual, ppd);
    if (status != HIDP_STATUS_SUCCESS)
    {
        out << L"HidP_GetValueCaps failed: 0x" << std::hex << std::uppercase
            << static_cast<ULONG>(status) << std::dec << L"\n";
        return;
    }

    for (USHORT i = 0; i < actual; ++i)
    {
        auto const& c = caps[i];
        out << L"[" << i << L"] UsagePage=0x" << std::hex << std::setw(4) << std::setfill(L'0')
            << c.UsagePage << L" ReportID=0x" << std::setw(2) << static_cast<unsigned>(c.ReportID)
            << L" BitField=0x" << std::setw(4) << c.BitField
            << L" LinkCollection=" << std::dec << c.LinkCollection
            << L" IsAlias=" << static_cast<unsigned>(c.IsAlias)
            << L" IsRange=" << static_cast<unsigned>(c.IsRange)
            << L" IsAbsolute=" << static_cast<unsigned>(c.IsAbsolute)
            << L" HasNull=" << static_cast<unsigned>(c.HasNull)
            << L" BitSize=" << c.BitSize
            << L" ReportCount=" << c.ReportCount << L"\n";

        out << L"    LogicalMin=" << c.LogicalMin << L" LogicalMax=" << c.LogicalMax
            << L" PhysicalMin=" << c.PhysicalMin << L" PhysicalMax=" << c.PhysicalMax
            << L" UnitsExp=" << c.UnitsExp << L" Units=0x" << std::hex << c.Units << std::dec << L"\n";

        if (c.IsRange)
        {
            out << L"    UsageMin=0x" << std::hex << c.Range.UsageMin
                << L" UsageMax=0x" << c.Range.UsageMax << std::dec << L"\n";
        }
        else
        {
            out << L"    Usage=0x" << std::hex << c.NotRange.Usage << std::dec << L"\n";
        }
    }
}

void print_link_nodes(std::wostream& out,
                      PHIDP_PREPARSED_DATA ppd,
                      USHORT count)
{
    out << L"\n=== Link Collection Nodes (" << count << L") ===\n";
    if (!count)
        return;

    std::vector<HIDP_LINK_COLLECTION_NODE> nodes(count);
    ULONG actual = count;
    auto status = HidP_GetLinkCollectionNodes(nodes.data(), &actual, ppd);
    if (status != HIDP_STATUS_SUCCESS)
    {
        out << L"HidP_GetLinkCollectionNodes failed: 0x" << std::hex << std::uppercase
            << static_cast<ULONG>(status) << std::dec << L"\n";
        return;
    }

    for (ULONG i = 0; i < actual; ++i)
    {
        auto const& n = nodes[i];
        out << L"[" << i << L"] UsagePage=0x" << std::hex << std::setw(4) << std::setfill(L'0')
            << n.LinkUsagePage << L" Usage=0x" << std::setw(4) << n.LinkUsage << std::dec
            << L" Parent=" << n.Parent
            << L" Children=" << n.NumberOfChildren
            << L" FirstChild=" << n.FirstChild
            << L" NextSibling=" << n.NextSibling
            << L" CollectionType=" << static_cast<unsigned>(n.CollectionType)
            << L" IsAlias=" << static_cast<unsigned>(n.IsAlias) << L"\n";
    }
}

bool dump_one(std::wostream& out, std::wstring const& path, std::wstring const& instanceId)
{
    HANDLE h = CreateFileW(path.c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        out << L"CreateFile failed for " << path << L" error=" << GetLastError() << L"\n";
        return false;
    }

    out << L"============================================================\n";
    out << L"InstanceId: " << instanceId << L"\n";
    out << L"Path: " << path << L"\n";

    HIDD_ATTRIBUTES attr{};
    attr.Size = sizeof(attr);
    if (HidD_GetAttributes(h, &attr))
    {
        out << L"VID: 0x" << std::hex << std::setw(4) << std::setfill(L'0') << attr.VendorID
            << L" PID: 0x" << std::setw(4) << attr.ProductID
            << L" Version: 0x" << std::setw(4) << attr.VersionNumber << std::dec << L"\n";
    }

    auto manufacturer = get_string(h, HidD_GetManufacturerString);
    auto product = get_string(h, HidD_GetProductString);
    auto serial = get_string(h, HidD_GetSerialNumberString);
    if (!manufacturer.empty()) out << L"ManufacturerString: " << manufacturer << L"\n";
    if (!product.empty()) out << L"ProductString: " << product << L"\n";
    if (!serial.empty()) out << L"SerialString: " << serial << L"\n";

    PHIDP_PREPARSED_DATA ppd = nullptr;
    if (!HidD_GetPreparsedData(h, &ppd) || !ppd)
    {
        out << L"HidD_GetPreparsedData failed, error=" << GetLastError() << L"\n";
        CloseHandle(h);
        return false;
    }

    HIDP_CAPS caps{};
    auto status = HidP_GetCaps(ppd, &caps);
    if (status != HIDP_STATUS_SUCCESS)
    {
        out << L"HidP_GetCaps failed: 0x" << std::hex << std::uppercase
            << static_cast<ULONG>(status) << std::dec << L"\n";
        HidD_FreePreparsedData(ppd);
        CloseHandle(h);
        return false;
    }

    out << L"\n=== HIDP_CAPS ===\n";
    out << L"UsagePage: 0x" << std::hex << std::setw(4) << std::setfill(L'0') << caps.UsagePage
        << L" Usage: 0x" << std::setw(4) << caps.Usage << std::dec << L"\n";
    out << L"InputReportByteLength: " << caps.InputReportByteLength << L"\n";
    out << L"OutputReportByteLength: " << caps.OutputReportByteLength << L"\n";
    out << L"FeatureReportByteLength: " << caps.FeatureReportByteLength << L"\n";
    out << L"NumberLinkCollectionNodes: " << caps.NumberLinkCollectionNodes << L"\n";
    out << L"InputButtonCaps: " << caps.NumberInputButtonCaps
        << L" InputValueCaps: " << caps.NumberInputValueCaps
        << L" InputDataIndices: " << caps.NumberInputDataIndices << L"\n";
    out << L"OutputButtonCaps: " << caps.NumberOutputButtonCaps
        << L" OutputValueCaps: " << caps.NumberOutputValueCaps
        << L" OutputDataIndices: " << caps.NumberOutputDataIndices << L"\n";
    out << L"FeatureButtonCaps: " << caps.NumberFeatureButtonCaps
        << L" FeatureValueCaps: " << caps.NumberFeatureValueCaps
        << L" FeatureDataIndices: " << caps.NumberFeatureDataIndices << L"\n";

    print_button_caps(out, HidP_Input, ppd, caps.NumberInputButtonCaps);
    print_value_caps(out, HidP_Input, ppd, caps.NumberInputValueCaps);
    print_button_caps(out, HidP_Output, ppd, caps.NumberOutputButtonCaps);
    print_value_caps(out, HidP_Output, ppd, caps.NumberOutputValueCaps);
    print_button_caps(out, HidP_Feature, ppd, caps.NumberFeatureButtonCaps);
    print_value_caps(out, HidP_Feature, ppd, caps.NumberFeatureValueCaps);
    print_link_nodes(out, ppd, caps.NumberLinkCollectionNodes);

    HidD_FreePreparsedData(ppd);
    CloseHandle(h);
    return true;
}

int run(std::wostream& out)
{
    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO set = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE)
    {
        out << L"SetupDiGetClassDevs failed: " << GetLastError() << L"\n";
        return 2;
    }

    int matches = 0;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &hidGuid, index, &iface))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }

        DWORD needed = 0;
        SP_DEVINFO_DATA info{};
        info.cbSize = sizeof(info);
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &needed, nullptr);
        if (!needed)
            continue;

        std::vector<BYTE> storage(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, needed, nullptr, &info))
            continue;

        auto instanceId = get_instance_id(set, info);
        auto haystack = upper(instanceId + L"\n" + detail->DevicePath);
        if (haystack.find(L"VID_041E") == std::wstring::npos ||
            haystack.find(L"PID_3278") == std::wstring::npos ||
            haystack.find(L"MI_00") == std::wstring::npos)
            continue;

        ++matches;
        dump_one(out, detail->DevicePath, instanceId);
    }

    SetupDiDestroyDeviceInfoList(set);
    out << L"\nMatched X4 MI_00 HID interfaces: " << matches << L"\n";
    out << L"Read-only diagnostic: no HID output or feature report was sent.\n";
    return matches ? 0 : 1;
}
}

int wmain()
{
    std::wostringstream report;
    report << L"Sound Blaster X4 Windows HID caps diagnostic\n";
    report << L"Target: VID_041E PID_3278 MI_00\n\n";

    int rc = run(report);
    std::wcout << report.str();

    std::wofstream file(L"x4-hid-diag.txt", std::ios::binary | std::ios::trunc);
    if (file)
    {
        file << report.str();
        file.close();
        std::wcout << L"\nSaved: x4-hid-diag.txt\n";
    }
    else
    {
        std::wcerr << L"Could not create x4-hid-diag.txt\n";
    }

    return rc;
}
