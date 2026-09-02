#include <windows.h>
#include <setupapi.h>

#include <array>
#include <cstdint>
#include <cwctype>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")

namespace
{
constexpr wchar_t kTargetHardwareId[] = L"USB\\VID_041E&PID_3278&MI_01";
constexpr std::array<std::uint8_t, 6> kDirectModeOff{
    0x5A, 0x39, 0x03, 0x00, 0x05, 0x00};
constexpr std::array<std::uint8_t, 6> kDirectModeOn{
    0x5A, 0x39, 0x03, 0x00, 0x05, 0x01};

std::wstring upper(std::wstring value)
{
    for (auto& ch : value)
        ch = static_cast<wchar_t>(std::towupper(ch));
    return value;
}

std::wstring get_reg_property(HDEVINFO set, SP_DEVINFO_DATA& info, DWORD property)
{
    DWORD type = 0;
    DWORD required = 0;
    SetupDiGetDeviceRegistryPropertyW(set, &info, property, &type, nullptr, 0, &required);
    if (required == 0)
        return {};

    std::vector<BYTE> buffer(required + sizeof(wchar_t) * 2, 0);
    if (!SetupDiGetDeviceRegistryPropertyW(
            set, &info, property, &type, buffer.data(),
            static_cast<DWORD>(buffer.size()), nullptr))
        return {};

    if (type != REG_SZ && type != REG_EXPAND_SZ && type != REG_MULTI_SZ)
        return {};

    auto const* text = reinterpret_cast<wchar_t const*>(buffer.data());
    if (type != REG_MULTI_SZ)
        return text;

    std::wstring joined;
    auto const* p = text;
    while (*p)
    {
        if (!joined.empty())
            joined += L" | ";
        std::wstring item = p;
        joined += item;
        p += item.size() + 1;
    }
    return joined;
}

std::wstring get_port_name(HDEVINFO set, SP_DEVINFO_DATA& info)
{
    HKEY key = SetupDiOpenDevRegKey(
        set, &info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE)
        return {};

    wchar_t value[64]{};
    DWORD type = 0;
    DWORD bytes = sizeof(value);
    LONG rc = RegQueryValueExW(
        key, L"PortName", nullptr, &type,
        reinterpret_cast<LPBYTE>(value), &bytes);
    RegCloseKey(key);

    if (rc != ERROR_SUCCESS || type != REG_SZ)
        return {};
    return value;
}

std::wstring find_x4_control_port()
{
    HDEVINFO set = SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE)
        return {};

    std::wstring result;
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

        auto ids = upper(get_reg_property(set, info, SPDRP_HARDWAREID));
        if (ids.find(kTargetHardwareId) == std::wstring::npos)
            continue;

        result = get_port_name(set, info);
        if (!result.empty())
            break;
    }

    SetupDiDestroyDeviceInfoList(set);
    return result;
}

std::wstring normalize_port(std::wstring port)
{
    if (port.rfind(L"\\\\.\\", 0) == 0)
        return port;
    return L"\\\\.\\" + port;
}

void print_command(std::array<std::uint8_t, 6> const& command)
{
    std::wcout << L"Command: ";
    for (auto byte : command)
    {
        std::wcout << std::uppercase << std::hex
                   << std::setw(2) << std::setfill(L'0')
                   << static_cast<unsigned>(byte);
    }
    std::wcout << std::dec << L"\n";
}

int send_command(std::wstring const& port,
                 std::array<std::uint8_t, 6> const& command,
                 bool turnOn)
{
    auto path = normalize_port(port);
    HANDLE handle = CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"Open failed for " << port
                   << L" (Win32 error " << GetLastError() << L")\n";
        return 20;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(handle, &timeouts);

    std::wcout << L"Port: " << port << L"\n";
    print_command(command);

    DWORD written = 0;
    BOOL ok = WriteFile(
        handle, command.data(), static_cast<DWORD>(command.size()),
        &written, nullptr);

    if (!ok)
    {
        DWORD error = GetLastError();
        CloseHandle(handle);
        std::wcerr << L"WriteFile failed (Win32 error " << error << L")\n";
        return 30;
    }

    FlushFileBuffers(handle);
    CloseHandle(handle);

    if (written != command.size())
    {
        std::wcerr << L"Short write: " << written << L" / "
                   << command.size() << L" bytes\n";
        return 31;
    }

    std::wcout << L"Wrote 6 raw bytes. Direct Mode "
               << (turnOn ? L"ON" : L"OFF")
               << L" command sent.\n";
    return 0;
}
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2 || argc > 3)
    {
        std::wcerr << L"Usage: x4-serial-poc.exe on|off [COMx]\n";
        return 2;
    }

    std::wstring mode = upper(argv[1]);
    bool turnOn = false;
    if (mode == L"ON")
        turnOn = true;
    else if (mode != L"OFF")
    {
        std::wcerr << L"Usage: x4-serial-poc.exe on|off [COMx]\n";
        return 2;
    }

    std::wstring port;
    if (argc == 3)
        port = argv[2];
    else
        port = find_x4_control_port();

    if (port.empty())
    {
        std::wcerr << L"Sound Blaster X4 control interface "
                   << kTargetHardwareId << L" was not found.\n";
        return 10;
    }

    auto const& command = turnOn ? kDirectModeOn : kDirectModeOff;
    return send_command(port, command, turnOn);
}
