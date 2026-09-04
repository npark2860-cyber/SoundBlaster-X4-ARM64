#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <string>
#include <vector>

#include "asio_callback_compat.h"

#if !defined(_M_ARM64EC)
#error B5 capability probe must be compiled as ARM64EC.
#endif

namespace {

struct DriverEntry {
    std::wstring key;
    std::wstring description;
    std::wstring clsid_text;
    CLSID clsid{};
    HKEY root = nullptr;
    REGSAM view = 0;
};

struct CallbackBuffer {
    void* buffer[2]{};
    size_t bytes = 0;
};

CallbackBuffer g_buffers[64]{};
volatile LONG g_buffer_count = 0;
volatile LONG g_callbacks = 0;

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), n, nullptr, nullptr);
    out.pop_back();
    return out;
}

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

bool contains_i(const std::wstring& a, const std::wstring& b) {
    return b.empty() || lower(a).find(lower(b)) != std::wstring::npos;
}

const char* root_name(HKEY root) {
    return root == HKEY_LOCAL_MACHINE ? "HKLM" : "HKCU";
}

const char* view_name(REGSAM view) {
    return view == KEY_WOW64_64KEY ? "64" : "32";
}

bool reg_string(HKEY key, const wchar_t* name, std::wstring* out) {
    DWORD type = 0, bytes = 0;
    if (!out ||
        RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || bytes < sizeof(wchar_t)) {
        return false;
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type,
                        reinterpret_cast<BYTE*>(buffer.data()), &bytes) != ERROR_SUCCESS) {
        return false;
    }
    *out = buffer.data();
    return true;
}

void enum_asio(HKEY root, REGSAM view, std::vector<DriverEntry>* out) {
    if (!out) return;
    HKEY asio = nullptr;
    if (RegOpenKeyExW(root, L"SOFTWARE\\ASIO", 0, KEY_READ | view, &asio) != ERROR_SUCCESS) return;

    for (DWORD index = 0;; ++index) {
        wchar_t name[256]{};
        DWORD chars = static_cast<DWORD>(std::size(name));
        if (const LONG rc = RegEnumKeyExW(asio, index, name, &chars, nullptr, nullptr, nullptr, nullptr);
            rc == ERROR_NO_MORE_ITEMS) {
            break;
        } else if (rc != ERROR_SUCCESS) {
            continue;
        }

        HKEY key = nullptr;
        if (RegOpenKeyExW(asio, name, 0, KEY_READ, &key) != ERROR_SUCCESS) continue;

        DriverEntry entry{};
        entry.key = name;
        entry.root = root;
        entry.view = view;
        const bool have_clsid = reg_string(key, L"CLSID", &entry.clsid_text);
        reg_string(key, L"Description", &entry.description);
        RegCloseKey(key);
        if (!have_clsid || FAILED(CLSIDFromString(entry.clsid_text.c_str(), &entry.clsid))) continue;

        bool duplicate = false;
        for (const auto& e : *out) {
            if (_wcsicmp(e.key.c_str(), entry.key.c_str()) == 0 &&
                _wcsicmp(e.clsid_text.c_str(), entry.clsid_text.c_str()) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) out->push_back(entry);
    }
    RegCloseKey(asio);
}

std::vector<DriverEntry> drivers() {
    std::vector<DriverEntry> out;
    for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        for (REGSAM view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) enum_asio(root, view, &out);
    }
    return out;
}

const char* errname(ASIOError e) {
    switch (e) {
        case ASE_OK: return "ASE_OK";
        case ASE_SUCCESS: return "ASE_SUCCESS";
        case ASE_NotPresent: return "ASE_NotPresent";
        case ASE_HWMalfunction: return "ASE_HWMalfunction";
        case ASE_InvalidParameter: return "ASE_InvalidParameter";
        case ASE_InvalidMode: return "ASE_InvalidMode";
        case ASE_SPNotAdvancing: return "ASE_SPNotAdvancing";
        case ASE_NoClock: return "ASE_NoClock";
        case ASE_NoMemory: return "ASE_NoMemory";
        default: return "UNKNOWN";
    }
}

const char* typename_asio(ASIOSampleType t) {
    switch (t) {
        case 0: return "Int16MSB"; case 1: return "Int24MSB"; case 2: return "Int32MSB";
        case 3: return "Float32MSB"; case 4: return "Float64MSB";
        case 8: return "Int32MSB16"; case 9: return "Int32MSB18";
        case 10: return "Int32MSB20"; case 11: return "Int32MSB24";
        case 16: return "Int16LSB"; case 17: return "Int24LSB"; case 18: return "Int32LSB";
        case 19: return "Float32LSB"; case 20: return "Float64LSB";
        case 24: return "Int32LSB16"; case 25: return "Int32LSB18";
        case 26: return "Int32LSB20"; case 27: return "Int32LSB24";
        case 32: return "DSDInt8LSB1"; case 33: return "DSDInt8MSB1"; case 40: return "DSDInt8NER8";
        default: return "UNKNOWN";
    }
}

size_t bytes_per_sample(ASIOSampleType t) {
    switch (t) {
        case 0: case 16: return 2;
        case 1: case 17: return 3;
        case 2: case 3: case 8: case 9: case 10: case 11:
        case 18: case 19: case 24: case 25: case 26: case 27: return 4;
        case 4: case 20: return 8;
        case 32: case 33: case 40: return 1;
        default: return 0;
    }
}

HRESULT create_driver(const DriverEntry& entry, IASIO** driver) {
    if (!driver) return E_POINTER;
    *driver = nullptr;
    return CoCreateInstance(entry.clsid, nullptr, CLSCTX_INPROC_SERVER,
                            entry.clsid, reinterpret_cast<void**>(driver));
}

void driver_error(IASIO* driver, const char* label) {
    char message[124]{};
    if (driver) driver->getErrorMessage(message);
    std::printf("%s=%s\n", label, message);
}

void result(const char* label, ASIOError e) {
    std::printf("%s=%ld (%s)\n", label, e, errname(e));
}

void channel(IASIO* driver, ASIOBool input, long index) {
    ASIOChannelInfo info{};
    info.channel = index;
    info.isInput = input;
    const ASIOError e = driver->getChannelInfo(&info);
    std::printf("channel dir=%s index=%ld result=%ld type=%ld(%s) active=%ld group=%ld name=%s\n",
                input ? "input" : "output", index, e, info.type, typename_asio(info.type),
                info.isActive, info.channelGroup, e == ASE_OK ? info.name : "");
}

bool report(const DriverEntry& entry) {
    std::puts("=== ASIO DRIVER REPORT BEGIN ===");
    std::printf("registry root=%s view=%s name=%s description=%s clsid=%s\n",
                root_name(entry.root), view_name(entry.view), utf8(entry.key).c_str(),
                utf8(entry.description).c_str(), utf8(entry.clsid_text).c_str());

    IASIO* driver = nullptr;
    const HRESULT hr = create_driver(entry, &driver);
    std::printf("CoCreateInstance=0x%08lX\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !driver) return false;

    char name[64]{};
    driver->getDriverName(name);
    std::printf("driverName=%s driverVersion=%ld\n", name, driver->getDriverVersion());

    const ASIOBool init = driver->init(GetConsoleWindow());
    std::printf("init=%ld\n", init);
    driver_error(driver, "initMessage");
    if (init != ASIOTrue) {
        driver->Release();
        return false;
    }

    long ins = 0, outs = 0;
    const ASIOError channels = driver->getChannels(&ins, &outs);
    result("getChannels", channels);
    std::printf("channels inputs=%ld outputs=%ld\n", ins, outs);

    long min = 0, max = 0, pref = 0, gran = 0;
    result("getBufferSize", driver->getBufferSize(&min, &max, &pref, &gran));
    std::printf("buffer min=%ld max=%ld preferred=%ld granularity=%ld\n", min, max, pref, gran);

    ASIOSampleRate current = 0;
    result("getSampleRate", driver->getSampleRate(&current));
    std::printf("currentSampleRate=%.0f\n", current);

    const ASIOSampleRate rates[] = {
        8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
        88200, 96000, 176400, 192000, 352800, 384000
    };
    for (const auto rate : rates) {
        const ASIOError e = driver->canSampleRate(rate);
        std::printf("canSampleRate %.0f=%ld (%s)\n", rate, e, errname(e));
    }

    ASIOClockSource clocks[32]{};
    long clock_count = static_cast<long>(std::size(clocks));
    const ASIOError clock_result = driver->getClockSources(clocks, &clock_count);
    result("getClockSources", clock_result);
    std::printf("clockCount=%ld\n", clock_count);
    if (clock_result == ASE_OK) {
        for (long i = 0; i < std::min<long>(clock_count, static_cast<long>(std::size(clocks))); ++i) {
            std::printf("clock ordinal=%ld index=%ld current=%ld channel=%ld group=%ld name=%s\n",
                        i, clocks[i].index, clocks[i].isCurrentSource,
                        clocks[i].associatedChannel, clocks[i].associatedGroup, clocks[i].name);
        }
    }

    if (channels == ASE_OK) {
        for (long i = 0; i < ins; ++i) channel(driver, ASIOTrue, i);
        for (long i = 0; i < outs; ++i) channel(driver, ASIOFalse, i);
    }

    long in_latency = 0, out_latency = 0;
    result("getLatencies", driver->getLatencies(&in_latency, &out_latency));
    std::printf("latency inputFrames=%ld outputFrames=%ld\n", in_latency, out_latency);
    result("future(kAsioCanTimeInfo)", driver->future(kAsioCanTimeInfo, nullptr));

    driver->Release();
    std::puts("=== ASIO DRIVER REPORT END ===");
    return true;
}

void clear_slot(long slot) {
    if (slot < 0 || slot > 1) return;
    const LONG count = std::min<LONG>(
        InterlockedCompareExchange(&g_buffer_count, 0, 0),
        static_cast<LONG>(std::size(g_buffers)));
    for (LONG i = 0; i < count; ++i) {
        if (g_buffers[i].buffer[slot] && g_buffers[i].bytes) {
            ZeroMemory(g_buffers[i].buffer[slot], g_buffers[i].bytes);
        }
    }
}

void buffer_switch(long slot, ASIOBool) {
    clear_slot(slot);
    InterlockedIncrement(&g_callbacks);
}

void sample_rate_changed(ASIOSampleRate) {}

long asio_message(long selector, long value, void*, double*) {
    if (selector == kAsioSelectorSupported) return value == kAsioEngineVersion ? 1 : 0;
    if (selector == kAsioEngineVersion) return 2;
    return 0;
}

bool lifecycle_cycle(const DriverEntry& entry, int ordinal) {
    IASIO* driver = nullptr;
    const HRESULT hr = create_driver(entry, &driver);
    std::printf("lifecycle cycle=%d create=0x%08lX\n", ordinal, static_cast<unsigned long>(hr));
    if (FAILED(hr) || !driver) return false;

    bool ok = false, created = false, started = false;
    do {
        if (driver->init(GetConsoleWindow()) != ASIOTrue) {
            driver_error(driver, "lifecycleInitMessage");
            break;
        }

        long ins = 0, outs = 0;
        if (driver->getChannels(&ins, &outs) != ASE_OK || outs < 1) break;

        long min = 0, max = 0, pref = 0, gran = 0;
        if (driver->getBufferSize(&min, &max, &pref, &gran) != ASE_OK || pref <= 0) break;

        const long use = std::min<long>(outs, 2);
        std::vector<ASIOBufferInfo> infos(static_cast<size_t>(use));
        std::vector<size_t> bytes(static_cast<size_t>(use));
        bool channel_ok = true;
        for (long i = 0; i < use; ++i) {
            ASIOChannelInfo ci{};
            ci.channel = i;
            ci.isInput = ASIOFalse;
            if (driver->getChannelInfo(&ci) != ASE_OK ||
                (bytes[static_cast<size_t>(i)] = bytes_per_sample(ci.type)) == 0) {
                driver_error(driver, "lifecycleChannelMessage");
                channel_ok = false;
                break;
            }
            infos[static_cast<size_t>(i)].isInput = ASIOFalse;
            infos[static_cast<size_t>(i)].channelNum = i;
        }
        if (!channel_ok) break;

        ASIOCallbacks callbacks{};
        callbacks.bufferSwitch = &buffer_switch;
        callbacks.sampleRateDidChange = &sample_rate_changed;
        callbacks.asioMessage = &asio_message;

        const ASIOError create = driver->createBuffers(infos.data(), use, pref, &callbacks);
        std::printf("lifecycle cycle=%d createBuffers=%ld preferred=%ld outputs=%ld\n",
                    ordinal, create, pref, use);
        if (create != ASE_OK) {
            driver_error(driver, "lifecycleCreateMessage");
            break;
        }
        created = true;

        ZeroMemory(g_buffers, sizeof(g_buffers));
        for (long i = 0; i < use; ++i) {
            g_buffers[i].buffer[0] = infos[static_cast<size_t>(i)].buffers[0];
            g_buffers[i].buffer[1] = infos[static_cast<size_t>(i)].buffers[1];
            g_buffers[i].bytes = static_cast<size_t>(pref) * bytes[static_cast<size_t>(i)];
        }
        InterlockedExchange(&g_buffer_count, use);
        InterlockedExchange(&g_callbacks, 0);
        clear_slot(0);
        clear_slot(1);

        long il = 0, ol = 0;
        const ASIOError latency = driver->getLatencies(&il, &ol);
        std::printf("lifecycle cycle=%d latencies=%ld inputFrames=%ld outputFrames=%ld\n",
                    ordinal, latency, il, ol);

        const ASIOError start = driver->start();
        std::printf("lifecycle cycle=%d start=%ld\n", ordinal, start);
        if (start != ASE_OK) {
            driver_error(driver, "lifecycleStartMessage");
            break;
        }
        started = true;
        Sleep(750);

        const LONG callbacks_seen = InterlockedCompareExchange(&g_callbacks, 0, 0);
        const ASIOError stop = driver->stop();
        started = false;
        std::printf("lifecycle cycle=%d stop=%ld callbacks=%ld\n",
                    ordinal, stop, callbacks_seen);
        driver_error(driver, "lifecycleStopMessage");
        ok = stop == ASE_OK && callbacks_seen > 0;
    } while (false);

    if (started) driver->stop();
    InterlockedExchange(&g_buffer_count, 0);
    ZeroMemory(g_buffers, sizeof(g_buffers));
    if (created && driver->disposeBuffers() != ASE_OK) ok = false;
    driver->Release();
    Sleep(250);
    return ok;
}

bool lifecycle(const DriverEntry& entry, int cycles) {
    std::printf("=== ASIO LIFECYCLE BEGIN cycles=%d ===\n", cycles);
    for (int i = 1; i <= cycles; ++i) {
        if (!lifecycle_cycle(entry, i)) {
            std::printf("=== ASIO LIFECYCLE END result=FAIL cycle=%d ===\n", i);
            return false;
        }
    }
    std::puts("=== ASIO LIFECYCLE END result=PASS ===");
    return true;
}

void list(const std::vector<DriverEntry>& entries) {
    std::puts("=== ASIO REGISTRY LIST BEGIN ===");
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        std::printf("entry=%zu root=%s view=%s name=%s description=%s clsid=%s\n",
                    i, root_name(e.root), view_name(e.view), utf8(e.key).c_str(),
                    utf8(e.description).c_str(), utf8(e.clsid_text).c_str());
    }
    std::puts("=== ASIO REGISTRY LIST END ===");
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(co) && co != RPC_E_CHANGED_MODE) return 2;

    const auto entries = drivers();
    list(entries);
    if (entries.empty()) {
        if (SUCCEEDED(co)) CoUninitialize();
        return 3;
    }

    std::wstring match;
    int cycles = 0;
    bool list_only = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--list") == 0) list_only = true;
        else if (_wcsicmp(argv[i], L"--match") == 0 && i + 1 < argc) match = argv[++i];
        else if (_wcsicmp(argv[i], L"--lifecycle") == 0 && i + 1 < argc) cycles = _wtoi(argv[++i]);
        else {
            std::puts("Usage: --list | --match <substring> [--lifecycle 0..20]");
            if (SUCCEEDED(co)) CoUninitialize();
            return 4;
        }
    }

    if (list_only && match.empty()) {
        if (SUCCEEDED(co)) CoUninitialize();
        return 0;
    }
    if (match.empty() || cycles < 0 || cycles > 20) {
        if (SUCCEEDED(co)) CoUninitialize();
        return 4;
    }

    std::vector<const DriverEntry*> matches;
    for (const auto& e : entries) {
        if (contains_i(e.key, match) || contains_i(e.description, match)) matches.push_back(&e);
    }
    if (matches.size() != 1) {
        std::printf("Driver match count=%zu for \"%s\"; expected exactly one.\n",
                    matches.size(), utf8(match).c_str());
        if (SUCCEEDED(co)) CoUninitialize();
        return 5;
    }

    bool pass = report(*matches[0]);
    if (pass && cycles > 0) pass = lifecycle(*matches[0], cycles);
    std::printf("B5 ASIO CAPABILITY PROBE RESULT: %s\n", pass ? "PASS" : "FAIL");

    if (SUCCEEDED(co)) CoUninitialize();
    return pass ? 0 : 6;
}
