#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr USHORT kVid = 0x041E;
constexpr USHORT kPid = 0x3278;
constexpr size_t kReportLength = 65;

std::wstring upper(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towupper(c));
    });
    return value;
}

std::wstring get_instance_id(HDEVINFO set, SP_DEVINFO_DATA& info)
{
    DWORD needed = 0;
    SetupDiGetDeviceInstanceIdW(set, &info, nullptr, 0, &needed);
    if (!needed)
        return {};

    std::vector<wchar_t> buffer(needed + 1, L'\0');
    if (!SetupDiGetDeviceInstanceIdW(set, &info, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr))
        return {};
    return buffer.data();
}

struct HidTarget
{
    std::wstring path;
    std::wstring instanceId;
    USHORT outputReportLength{};
};

bool find_x4_hid(HidTarget& result)
{
    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO set = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE)
        return false;

    bool found = false;
    for (DWORD index = 0; !found; ++index)
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
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &needed, nullptr);
        if (!needed)
            continue;

        std::vector<BYTE> storage(needed, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA info{};
        info.cbSize = sizeof(info);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, needed, nullptr, &info))
            continue;

        std::wstring instanceId = get_instance_id(set, info);
        std::wstring idUpper = upper(instanceId);
        if (idUpper.find(L"VID_041E&PID_3278&MI_00") == std::wstring::npos)
            continue;

        HANDLE h = CreateFileW(detail->DevicePath,
                               0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr,
                               OPEN_EXISTING,
                               0,
                               nullptr);
        if (h == INVALID_HANDLE_VALUE)
            continue;

        HIDD_ATTRIBUTES attributes{};
        attributes.Size = sizeof(attributes);
        bool identityOk = HidD_GetAttributes(h, &attributes) &&
                          attributes.VendorID == kVid && attributes.ProductID == kPid;

        USHORT outputLength = 0;
        PHIDP_PREPARSED_DATA ppd = nullptr;
        if (identityOk && HidD_GetPreparsedData(h, &ppd) && ppd)
        {
            HIDP_CAPS caps{};
            if (HidP_GetCaps(ppd, &caps) == HIDP_STATUS_SUCCESS)
                outputLength = caps.OutputReportByteLength;
            HidD_FreePreparsedData(ppd);
        }
        CloseHandle(h);

        if (!identityOk)
            continue;

        result.path = detail->DevicePath;
        result.instanceId = std::move(instanceId);
        result.outputReportLength = outputLength;
        found = true;
    }

    SetupDiDestroyDeviceInfoList(set);
    return found;
}

void print_hex(std::array<UCHAR, kReportLength> const& report)
{
    std::wcout << L"Report: ";
    for (size_t i = 0; i < report.size(); ++i)
    {
        std::wcout << std::hex << std::uppercase << std::setw(2) << std::setfill(L'0')
                   << static_cast<unsigned>(report[i]);
        if (i + 1 != report.size())
            std::wcout << L' ';
    }
    std::wcout << std::dec << L"\n";
}

int run(bool enable, bool useWriteFile)
{
    HidTarget target;
    if (!find_x4_hid(target))
    {
        std::wcerr << L"X4 MI_00 HID interface was not found.\n";
        return 2;
    }

    std::wcout << L"Instance: " << target.instanceId << L"\n";
    std::wcout << L"Path: " << target.path << L"\n";
    std::wcout << L"OutputReportByteLength: " << target.outputReportLength << L"\n";

    if (target.outputReportLength != kReportLength)
    {
        std::wcerr << L"Refusing to send: expected a 65-byte HID output report.\n";
        return 3;
    }

    std::array<UCHAR, kReportLength> report{};
    report[0] = 0x00; // Report ID 0.

    std::array<UCHAR, 6> command = enable
        ? std::array<UCHAR, 6>{0x5A, 0x39, 0x03, 0x00, 0x05, 0x01}
        : std::array<UCHAR, 6>{0x5A, 0x39, 0x03, 0x00, 0x05, 0x00};

    std::copy(command.begin(), command.end(), report.begin() + 1);

    std::wcout << L"Direct Mode: " << (enable ? L"ON" : L"OFF") << L"\n";
    std::wcout << L"Method: " << (useWriteFile ? L"WriteFile" : L"HidD_SetOutputReport") << L"\n";
    print_hex(report);

    HANDLE h = CreateFileW(target.path.c_str(),
                           GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr,
                           OPEN_EXISTING,
                           0,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"CreateFile(GENERIC_WRITE) failed. error=" << GetLastError() << L"\n";
        return 4;
    }

    bool ok = false;
    if (useWriteFile)
    {
        DWORD written = 0;
        ok = WriteFile(h, report.data(), static_cast<DWORD>(report.size()), &written, nullptr) != FALSE;
        if (!ok)
        {
            DWORD error = GetLastError();
            CloseHandle(h);
            std::wcerr << L"WriteFile failed. error=" << error << L"\n";
            return 5;
        }
        if (written != report.size())
        {
            CloseHandle(h);
            std::wcerr << L"WriteFile wrote " << written << L" bytes, expected " << report.size() << L".\n";
            return 6;
        }
        std::wcout << L"WriteFile accepted 65 bytes.\n";
    }
    else
    {
        ok = HidD_SetOutputReport(h, report.data(), static_cast<ULONG>(report.size())) != FALSE;
        if (!ok)
        {
            DWORD error = GetLastError();
            CloseHandle(h);
            std::wcerr << L"HidD_SetOutputReport failed. error=" << error << L"\n";
            return 7;
        }
        std::wcout << L"HidD_SetOutputReport accepted 65 bytes.\n";
    }

    CloseHandle(h);
    std::wcout << L"Observe the physical X4 state; API success only means Windows accepted the report.\n";
    return 0;
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2 || argc > 3)
    {
        std::wcerr << L"Usage:\n"
                   << L"  x4-hid-output-poc.exe on [setoutput|write]\n"
                   << L"  x4-hid-output-poc.exe off [setoutput|write]\n";
        return 1;
    }

    std::wstring state = upper(argv[1]);
    if (state != L"ON" && state != L"OFF")
    {
        std::wcerr << L"First argument must be on or off.\n";
        return 1;
    }

    bool useWriteFile = false;
    if (argc == 3)
    {
        std::wstring method = upper(argv[2]);
        if (method == L"WRITE")
            useWriteFile = true;
        else if (method != L"SETOUTPUT")
        {
            std::wcerr << L"Method must be setoutput or write.\n";
            return 1;
        }
    }

    return run(state == L"ON", useWriteFile);
}
