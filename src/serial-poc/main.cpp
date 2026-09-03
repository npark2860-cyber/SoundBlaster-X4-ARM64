#include <windows.h>
#include <setupapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace
{
constexpr wchar_t kTargetHardwareId[] = L"USB\\VID_041E&PID_3278&MI_01";
constexpr std::array<std::uint8_t, 3> kMaxPayloadQuery{0x5A, 0x03, 0x00};
constexpr std::array<std::uint8_t, 18> kUnlockHello{
    'w','h','o','a','r','e','y','o','u','.',
    'M','y','A','p','p','8','\r','\n'};

struct Logger
{
    std::ofstream file{"x4-ctcdc-probe.txt", std::ios::binary | std::ios::trunc};

    void line(std::string const& text = {})
    {
        std::cout << text << "\n";
        if (file)
            file << text << "\r\n";
    }
};

std::wstring upper(std::wstring value)
{
    for (auto& ch : value)
        ch = static_cast<wchar_t>(std::towupper(ch));
    return value;
}

std::string narrow_ascii(std::wstring const& value)
{
    std::string result;
    result.reserve(value.size());
    for (auto ch : value)
        result.push_back(ch >= 0 && ch < 128 ? static_cast<char>(ch) : '?');
    return result;
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

std::string hex_bytes(std::uint8_t const* data, std::size_t size)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i)
    {
        if (i)
            out << ' ';
        out << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return out.str();
}

std::string hex_bytes(std::vector<std::uint8_t> const& data)
{
    return hex_bytes(data.data(), data.size());
}

std::string ascii_bytes(std::vector<std::uint8_t> const& data)
{
    std::string out;
    out.reserve(data.size());
    for (auto b : data)
    {
        if (b == '\r') out += "\\r";
        else if (b == '\n') out += "\\n";
        else if (b >= 0x20 && b <= 0x7E) out.push_back(static_cast<char>(b));
        else out.push_back('.');
    }
    return out;
}

bool configure_like_ctcdc(HANDLE handle, Logger& log)
{
    DWORD oldMask = 0;
    if (!GetCommMask(handle, &oldMask))
    {
        log.line("GetCommMask failed: " + std::to_string(GetLastError()));
        return false;
    }
    if (!SetCommMask(handle, EV_RXCHAR | EV_TXEMPTY))
    {
        log.line("SetCommMask(0x05) failed: " + std::to_string(GetLastError()));
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb))
    {
        log.line("GetCommState failed: " + std::to_string(GetLastError()));
        return false;
    }

    log.line("Original DCB: baud=" + std::to_string(dcb.BaudRate) +
             " byteSize=" + std::to_string(dcb.ByteSize) +
             " parity=" + std::to_string(dcb.Parity) +
             " stopBits=" + std::to_string(dcb.StopBits));

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;

    if (!SetCommState(handle, &dcb))
    {
        log.line("SetCommState(115200/8N1) failed: " + std::to_string(GetLastError()));
        return false;
    }

    COMMTIMEOUTS timeouts{};
    if (!GetCommTimeouts(handle, &timeouts))
    {
        log.line("GetCommTimeouts failed: " + std::to_string(GetLastError()));
        return false;
    }
    timeouts = {};
    if (!SetCommTimeouts(handle, &timeouts))
    {
        log.line("SetCommTimeouts(all zero) failed: " + std::to_string(GetLastError()));
        return false;
    }

    if (!PurgeComm(handle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR))
    {
        log.line("PurgeComm(0x0F) failed: " + std::to_string(GetLastError()));
        return false;
    }

    if (!EscapeCommFunction(handle, SETDTR))
    {
        log.line("EscapeCommFunction(SETDTR) failed: " + std::to_string(GetLastError()));
        return false;
    }

    log.line("Applied CTCDC serial init: mask=0x05, 115200/8N1, preserved unrelated DCB flags, zero timeouts, purge=0x0F, SETDTR.");
    return true;
}

template <std::size_t N>
bool write_exact(HANDLE handle, std::array<std::uint8_t, N> const& data, Logger& log, char const* label)
{
    log.line(std::string("TX ") + label + " (" + std::to_string(N) + " bytes): " + hex_bytes(data.data(), N));
    DWORD written = 0;
    if (!WriteFile(handle, data.data(), static_cast<DWORD>(N), &written, nullptr))
    {
        log.line(std::string("WriteFile failed: ") + std::to_string(GetLastError()));
        return false;
    }
    if (written != N)
    {
        log.line("Short write: " + std::to_string(written) + " / " + std::to_string(N));
        return false;
    }
    FlushFileBuffers(handle);
    return true;
}

std::vector<std::uint8_t> collect_available(HANDLE handle, DWORD totalWaitMs, DWORD quietMs, Logger& log)
{
    std::vector<std::uint8_t> result;
    auto const start = std::chrono::steady_clock::now();
    auto lastData = start;
    bool receivedAny = false;

    while (true)
    {
        DWORD errors = 0;
        COMSTAT stat{};
        if (!ClearCommError(handle, &errors, &stat))
        {
            log.line("ClearCommError failed: " + std::to_string(GetLastError()));
            break;
        }

        if (stat.cbInQue)
        {
            std::vector<std::uint8_t> chunk(stat.cbInQue);
            DWORD read = 0;
            if (!ReadFile(handle, chunk.data(), stat.cbInQue, &read, nullptr))
            {
                log.line("ReadFile failed: " + std::to_string(GetLastError()));
                break;
            }
            chunk.resize(read);
            result.insert(result.end(), chunk.begin(), chunk.end());
            receivedAny = true;
            lastData = std::chrono::steady_clock::now();
        }

        auto const now = std::chrono::steady_clock::now();
        auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        auto const quiet = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastData).count();
        if (elapsed >= totalWaitMs || (receivedAny && quiet >= quietMs))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return result;
}

bool find_max_payload_response(std::vector<std::uint8_t> const& data, std::uint16_t& maxPayload)
{
    for (std::size_t i = 0; i + 3 <= data.size(); ++i)
    {
        if (data[i] == 0x5A)
        {
            auto const len = static_cast<std::size_t>(data[i + 2]);
            if (i + 3 + len > data.size())
                continue;
            if (data[i + 1] == 0x03 && len == 2)
            {
                maxPayload = static_cast<std::uint16_t>(data[i + 3] | (data[i + 4] << 8));
                return true;
            }
        }
        else if (data[i] == 0x5B && i + 4 <= data.size())
        {
            auto const len = static_cast<std::size_t>(data[i + 2] | (data[i + 3] << 8));
            if (i + 4 + len > data.size())
                continue;
            if (data[i + 1] == 0x03 && len == 2)
            {
                maxPayload = static_cast<std::uint16_t>(data[i + 4] | (data[i + 5] << 8));
                return true;
            }
        }
    }
    return false;
}

bool starts_with_ascii(std::vector<std::uint8_t> const& data, char const* text)
{
    auto const n = std::char_traits<char>::length(text);
    return data.size() >= n && std::equal(text, text + n, data.begin());
}
}

int wmain(int argc, wchar_t* argv[])
{
    Logger log;
    log.line("Sound Blaster X4 CTCDC serial initialization probe");
    log.line("Read/diagnostic only after harmless protocol queries; no Direct Mode change is sent.");

    if (argc > 2)
    {
        log.line("Usage: x4-serial-ctcdc-probe.exe [COMx]");
        return 2;
    }

    std::wstring port = argc == 2 ? argv[1] : find_x4_control_port();
    if (port.empty())
    {
        log.line("Sound Blaster X4 MI_01 serial interface was not found.");
        return 10;
    }

    log.line("Port: " + narrow_ascii(port));
    auto const path = normalize_port(port);
    HANDLE handle = CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        log.line("CreateFileW failed: " + std::to_string(GetLastError()));
        return 20;
    }

    int result = 0;
    if (!configure_like_ctcdc(handle, log))
    {
        result = 21;
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!write_exact(handle, kMaxPayloadQuery, log, "maximum payload size query"))
        {
            result = 30;
        }
        else
        {
            auto response = collect_available(handle, 1500, 120, log);
            log.line("RX max-payload stage bytes=" + std::to_string(response.size()));
            if (!response.empty())
            {
                log.line("RX HEX: " + hex_bytes(response));
                log.line("RX ASCII: " + ascii_bytes(response));
            }

            std::uint16_t maxPayload = 0;
            if (find_max_payload_response(response, maxPayload))
            {
                std::ostringstream text;
                text << "VALID maximum payload size response: " << std::dec << maxPayload
                     << " (0x" << std::uppercase << std::hex << std::setw(4)
                     << std::setfill('0') << maxPayload << ")";
                log.line(text.str());
                log.line("CTCDC would continue Open without entering unlock/SetSwMode1 in this state.");
            }
            else
            {
                log.line("No valid 5A/5B command-0x03 maximum-payload response. CTCDC would enter its unlock path here.");
                PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
                if (!write_exact(handle, kUnlockHello, log, "unlock hello"))
                {
                    result = 31;
                }
                else
                {
                    auto unlockResponse = collect_available(handle, 3500, 150, log);
                    log.line("RX unlock stage bytes=" + std::to_string(unlockResponse.size()));
                    if (!unlockResponse.empty())
                    {
                        log.line("RX HEX: " + hex_bytes(unlockResponse));
                        log.line("RX ASCII: " + ascii_bytes(unlockResponse));
                    }

                    if (starts_with_ascii(unlockResponse, "whoareyou"))
                    {
                        log.line("Recognized CTCDC unlock challenge prefix 'whoareyou'.");
                        log.line("Probe stops before generating/sending the cryptographic unlock reply.");
                    }
                    else if (starts_with_ascii(unlockResponse, "NotYet\r\n"))
                    {
                        log.line("Device replied NotYet; CTCDC normally retries after a delay.");
                    }
                    else if (starts_with_ascii(unlockResponse, "Unknown command\r\n"))
                    {
                        log.line("Device replied Unknown command.");
                    }
                    else
                    {
                        log.line("Unlock response was empty or not one of the known CTCDC prefixes.");
                    }
                }
            }
        }
    }

    EscapeCommFunction(handle, CLRDTR);
    CloseHandle(handle);
    log.line("Done. Upload x4-ctcdc-probe.txt for analysis.");
    return result;
}
