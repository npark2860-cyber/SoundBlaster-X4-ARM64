#include <windows.h>
#include <setupapi.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace {
constexpr wchar_t kTargetHardwareId[] = L"USB\\VID_041E&PID_3278&MI_01";
constexpr std::uint8_t kCmdMaxPayload = 0x03;
constexpr std::uint8_t kCmdFirmware = 0x09;
constexpr std::uint8_t kCmdMalcolmGet = 0x11;
constexpr std::uint8_t kCmdAudioControlInfo = 0x21;
constexpr std::uint8_t kCmdButtons = 0x26;
constexpr std::uint8_t kCmdGraphicEq = 0x44;
constexpr std::uint8_t kCmdSoundMode = 0xA7;
constexpr std::uint8_t kDevCommGet = 0x01;
constexpr std::uint8_t kVoiceInputManager = 0x95;
constexpr std::uint8_t kPlaybackManager = 0x96;

struct Logger {
    std::ofstream file{"X4_READONLY_CAPABILITY_REPORT.txt", std::ios::binary | std::ios::trunc};
    void line(std::string const& text = {}) {
        std::cout << text << "\n";
        if (file) file << text << "\r\n";
    }
};
struct Query { std::string group, label; std::uint8_t command; std::vector<std::uint8_t> payload; };
struct Frame { std::uint8_t command; std::vector<std::uint8_t> payload; };

std::wstring upper(std::wstring value) {
    for (auto& ch : value) ch = static_cast<wchar_t>(std::towupper(ch));
    return value;
}
std::string narrow_ascii(std::wstring const& value) {
    std::string result;
    result.reserve(value.size());
    for (auto ch : value) result.push_back(ch >= 0 && ch < 128 ? static_cast<char>(ch) : '?');
    return result;
}
std::wstring get_reg_property(HDEVINFO set, SP_DEVINFO_DATA& info, DWORD property) {
    DWORD type = 0, required = 0;
    SetupDiGetDeviceRegistryPropertyW(set, &info, property, &type, nullptr, 0, &required);
    if (!required) return {};
    std::vector<BYTE> buffer(required + sizeof(wchar_t) * 2, 0);
    if (!SetupDiGetDeviceRegistryPropertyW(set, &info, property, &type, buffer.data(),
            static_cast<DWORD>(buffer.size()), nullptr)) return {};
    if (type != REG_SZ && type != REG_EXPAND_SZ && type != REG_MULTI_SZ) return {};
    auto const* text = reinterpret_cast<wchar_t const*>(buffer.data());
    if (type != REG_MULTI_SZ) return text;
    std::wstring joined;
    for (auto const* p = text; *p;) {
        if (!joined.empty()) joined += L" | ";
        std::wstring item = p;
        joined += item;
        p += item.size() + 1;
    }
    return joined;
}
std::wstring get_port_name(HDEVINFO set, SP_DEVINFO_DATA& info) {
    HKEY key = SetupDiOpenDevRegKey(set, &info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE) return {};
    wchar_t value[64]{};
    DWORD type = 0, bytes = sizeof(value);
    LONG const rc = RegQueryValueExW(key, L"PortName", nullptr, &type,
        reinterpret_cast<LPBYTE>(value), &bytes);
    RegCloseKey(key);
    return (rc == ERROR_SUCCESS && type == REG_SZ) ? std::wstring(value) : std::wstring{};
}
std::wstring find_x4_control_port() {
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return {};
    std::wstring result;
    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA info{}; info.cbSize = sizeof(info);
        if (!SetupDiEnumDeviceInfo(set, index, &info)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }
        if (upper(get_reg_property(set, info, SPDRP_HARDWAREID)).find(kTargetHardwareId) == std::wstring::npos) continue;
        result = get_port_name(set, info);
        if (!result.empty()) break;
    }
    SetupDiDestroyDeviceInfoList(set);
    return result;
}
std::wstring normalize_port(std::wstring port) {
    return port.rfind(L"\\\\.\\", 0) == 0 ? port : L"\\\\.\\" + port;
}
std::string hex_bytes(std::uint8_t const* data, std::size_t size) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        if (i) out << ' ';
        out << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return out.str();
}
std::string hex_bytes(std::vector<std::uint8_t> const& data) {
    return data.empty() ? std::string{} : hex_bytes(data.data(), data.size());
}
std::string byte_hex(std::uint8_t value) { return hex_bytes(&value, 1); }
std::string ascii_bytes(std::vector<std::uint8_t> const& data) {
    std::string out;
    for (auto b : data) {
        if (b == '\r') out += "\\r";
        else if (b == '\n') out += "\\n";
        else if (b >= 0x20 && b <= 0x7E) out.push_back(static_cast<char>(b));
        else out.push_back('.');
    }
    return out;
}

bool configure_like_ctcdc(HANDLE handle, Logger& log) {
    DWORD oldMask = 0;
    if (!GetCommMask(handle, &oldMask) || !SetCommMask(handle, EV_RXCHAR | EV_TXEMPTY)) {
        log.line("COM event-mask setup failed: " + std::to_string(GetLastError())); return false;
    }
    DCB dcb{}; dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb)) { log.line("GetCommState failed: " + std::to_string(GetLastError())); return false; }
    dcb.BaudRate = CBR_115200; dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_DISABLE; dcb.fRtsControl = RTS_CONTROL_DISABLE;
    if (!SetCommState(handle, &dcb)) { log.line("SetCommState failed: " + std::to_string(GetLastError())); return false; }
    COMMTIMEOUTS timeouts{};
    if (!SetCommTimeouts(handle, &timeouts)) { log.line("SetCommTimeouts failed: " + std::to_string(GetLastError())); return false; }
    if (!PurgeComm(handle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR)) {
        log.line("PurgeComm failed: " + std::to_string(GetLastError())); return false;
    }
    if (!EscapeCommFunction(handle, SETDTR)) { log.line("SETDTR failed: " + std::to_string(GetLastError())); return false; }
    log.line("CTCDC serial init: mask=0x05, 115200/8N1, zero timeouts, purge=0x0F, SETDTR.");
    return true;
}

std::vector<std::uint8_t> make_frame(Query const& q) {
    std::vector<std::uint8_t> frame{0x5A, q.command, static_cast<std::uint8_t>(q.payload.size())};
    frame.insert(frame.end(), q.payload.begin(), q.payload.end());
    return frame;
}
bool write_query(HANDLE handle, Query const& q, Logger& log) {
    auto const frame = make_frame(q);
    log.line("TX [" + q.group + "] " + q.label + ": " + hex_bytes(frame));
    DWORD written = 0;
    if (!WriteFile(handle, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr) || written != frame.size()) {
        log.line("WriteFile failed/short: error=" + std::to_string(GetLastError()) + " written=" + std::to_string(written));
        return false;
    }
    FlushFileBuffers(handle);
    return true;
}
std::vector<std::uint8_t> collect_available(HANDLE handle, DWORD totalWaitMs, DWORD quietMs, Logger& log) {
    std::vector<std::uint8_t> result;
    auto const start = std::chrono::steady_clock::now();
    auto lastData = start;
    bool gotAny = false;
    while (true) {
        DWORD errors = 0; COMSTAT stat{};
        if (!ClearCommError(handle, &errors, &stat)) { log.line("ClearCommError failed: " + std::to_string(GetLastError())); break; }
        if (stat.cbInQue) {
            std::vector<std::uint8_t> chunk(stat.cbInQue); DWORD read = 0;
            if (!ReadFile(handle, chunk.data(), stat.cbInQue, &read, nullptr)) { log.line("ReadFile failed: " + std::to_string(GetLastError())); break; }
            chunk.resize(read); result.insert(result.end(), chunk.begin(), chunk.end());
            gotAny = true; lastData = std::chrono::steady_clock::now();
        }
        auto const now = std::chrono::steady_clock::now();
        auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        auto const quiet = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastData).count();
        if (elapsed >= totalWaitMs || (gotAny && quiet >= quietMs)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return result;
}
std::vector<Frame> parse_frames(std::vector<std::uint8_t> const& data) {
    std::vector<Frame> frames;
    for (std::size_t i = 0; i < data.size();) {
        if (data[i] == 0x5A && i + 3 <= data.size()) {
            auto const len = static_cast<std::size_t>(data[i + 2]);
            if (i + 3 + len <= data.size()) { frames.push_back({data[i + 1], {data.begin()+i+3, data.begin()+i+3+len}}); i += 3 + len; continue; }
        }
        if (data[i] == 0x5B && i + 4 <= data.size()) {
            auto const len = static_cast<std::size_t>(data[i+2] | (data[i+3] << 8));
            if (i + 4 + len <= data.size()) { frames.push_back({data[i + 1], {data.begin()+i+4, data.begin()+i+4+len}}); i += 4 + len; continue; }
        }
        ++i;
    }
    return frames;
}
std::vector<Frame> transact_capture(HANDLE handle, Query const& q, Logger& log, DWORD wait = 1000, DWORD quiet = 60) {
    if (!write_query(handle, q, log)) return {};
    auto const rx = collect_available(handle, wait, quiet, log);
    log.line("RX bytes=" + std::to_string(rx.size()));
    if (rx.empty()) { log.line("RX: <no response>"); return {}; }
    log.line("RX HEX: " + hex_bytes(rx)); log.line("RX ASCII: " + ascii_bytes(rx));
    auto const frames = parse_frames(rx);
    if (frames.empty()) log.line("RX parse: no complete 5A/5B frame");
    for (std::size_t i = 0; i < frames.size(); ++i)
        log.line("RX frame[" + std::to_string(i) + "] cmd=0x" + byte_hex(frames[i].command) + " payload=" + hex_bytes(frames[i].payload));
    return frames;
}
bool transact(HANDLE handle, Query const& q, Logger& log) {
    if (!write_query(handle, q, log)) return false;
    auto const rx = collect_available(handle, 1000, 60, log);
    log.line("RX bytes=" + std::to_string(rx.size()));
    if (rx.empty()) { log.line("RX: <no response>"); return true; }
    log.line("RX HEX: " + hex_bytes(rx)); log.line("RX ASCII: " + ascii_bytes(rx));
    auto const frames = parse_frames(rx);
    if (frames.empty()) log.line("RX parse: no complete 5A/5B frame");
    for (std::size_t i = 0; i < frames.size(); ++i)
        log.line("RX frame[" + std::to_string(i) + "] cmd=0x" + byte_hex(frames[i].command) + " payload=" + hex_bytes(frames[i].payload));
    return true;
}
Query malcolm(std::string group, std::uint8_t module, std::uint8_t param) {
    return {std::move(group), "module=0x" + byte_hex(module) + " param=0x" + byte_hex(param), kCmdMalcolmGet, {kDevCommGet, module, param}};
}
std::vector<Query> build_queries() {
    std::vector<Query> q{
        {"Session","GetMaximumPayloadSize",kCmdMaxPayload,{}},
        {"Session","GetFirmwareVersionString",kCmdFirmware,{0x02}},
        {"Session","QueryButtonsAvailable",kCmdButtons,{0x05}}
    };
    for (std::uint8_t p = 0; p <= 20; ++p) q.push_back(malcolm("AcousticEngine", kPlaybackManager, p));
    for (std::uint8_t p = 23; p <= 25; ++p) q.push_back(malcolm("AcousticEngine", kPlaybackManager, p));
    for (std::uint8_t p = 0; p <= 45; ++p) q.push_back(malcolm("CrystalVoice", kVoiceInputManager, p));
    for (auto op : {std::uint8_t{0x02},std::uint8_t{0x03},std::uint8_t{0x04},std::uint8_t{0x06},std::uint8_t{0x0A},std::uint8_t{0x0B},std::uint8_t{0x0E}})
        q.push_back({"GraphicEQ","operation=0x" + byte_hex(op),kCmdGraphicEq,{op}});
    q.push_back({"Mixer","GetAudioControlInformation",kCmdAudioControlInfo,{}});
    q.push_back({"SoundMode","GetActiveSoundMode",kCmdSoundMode,{0x01}});
    q.push_back({"SoundMode","GetSoundModeSupport",kCmdSoundMode,{0x02}});
    return q;
}
}

int wmain(int argc, wchar_t* argv[]) {
    Logger log;
    log.line("Sound Blaster X4 ARM64 read-only capability probe");
    log.line("READ-ONLY: no Direct Mode setter, no Malcolm SET(0x12), no EQ/SoundMode SET operation, no raw-command CLI.");
    if (argc > 2) { log.line("Usage: x4-control-readonly-probe.exe [COMx]"); return 2; }
    std::wstring const port = argc == 2 ? argv[1] : find_x4_control_port();
    if (port.empty()) { log.line("X4 MI_01 serial interface not found."); return 10; }
    log.line("Port: " + narrow_ascii(port));
    HANDLE const handle = CreateFileW(normalize_port(port).c_str(), GENERIC_READ|GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { log.line("CreateFileW failed: " + std::to_string(GetLastError())); return 20; }

    int result = 0; std::size_t completed = 0;
    if (!configure_like_ctcdc(handle, log)) result = 21;
    else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto const queries = build_queries();
        log.line(); log.line("=== Session ===");
        auto const maxFrames = transact_capture(handle, queries[0], log, 3000, 120);
        auto const maxIt = std::find_if(maxFrames.begin(), maxFrames.end(), [](Frame const& f){ return f.command==kCmdMaxPayload && f.payload.size()==2; });
        if (maxIt == maxFrames.end()) { log.line("STOP: Maximum Payload Size did not validate. No capability queries sent."); result = 31; }
        else {
            auto const maxPayload = static_cast<std::uint16_t>(maxIt->payload[0] | (maxIt->payload[1] << 8));
            log.line("VALID Maximum Payload Size: " + std::to_string(maxPayload)); ++completed;
            auto const fwFrames = transact_capture(handle, queries[1], log, 3000, 120);
            auto const fwIt = std::find_if(fwFrames.begin(), fwFrames.end(), [](Frame const& f){ return f.command==kCmdFirmware && !f.payload.empty() && f.payload[0]==0x02; });
            if (fwIt == fwFrames.end()) { log.line("STOP: firmware response did not validate. No capability queries sent."); result = 32; }
            else {
                log.line("VALID firmware response framing."); ++completed;
                if (!transact(handle, queries[2], log)) result = 33;
                else {
                    ++completed; std::string group;
                    for (std::size_t i=3; i<queries.size(); ++i) {
                        auto const& query = queries[i];
                        if (query.group != group) { group=query.group; log.line(); log.line("=== " + group + " ==="); }
                        if (!transact(handle, query, log)) { result=30; break; }
                        ++completed;
                    }
                }
            }
        }
        log.line(); log.line("Completed read-only queries: " + std::to_string(completed) + " / " + std::to_string(queries.size()));
    }
    CloseHandle(handle);
    if (!result) log.line("Probe complete. Upload X4_READONLY_CAPABILITY_REPORT.txt.");
    else log.line("Probe stopped with error code " + std::to_string(result) + ".");
    return result;
}
