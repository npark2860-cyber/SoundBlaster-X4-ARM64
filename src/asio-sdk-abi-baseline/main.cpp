#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <ks.h>
#include <mmreg.h>
#include <ksmedia.h>

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cwchar>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ksuser.lib")

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error This baseline must be built for native Windows ARM64, not ARM64EC.
#endif

namespace {

constexpr ULONG kRenderPinId = 1;
constexpr ULONG kSampleRate = 48000;
constexpr WORD  kChannels = 2;
constexpr WORD  kBitsPerSample = 16;
constexpr ULONG kBytesPerFrame = 4;
constexpr ULONG kRequestedBufferBytes = 4096;
constexpr ULONG kNotificationCount = 2;
constexpr ULONG kNotificationsPerRun = 20;

FILE* g_log = nullptr;

void logf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    std::vprintf(fmt, args);
    std::printf("\n");
    std::fflush(stdout);

    if (g_log) {
        std::vfprintf(g_log, fmt, args_copy);
        std::fprintf(g_log, "\n");
        std::fflush(g_log);
    }

    va_end(args_copy);
    va_end(args);
}

bool contains_ascii_i(const wchar_t* text, const wchar_t* needle) {
    if (!text || !needle || !*needle) return false;
    for (const wchar_t* p = text; *p; ++p) {
        const wchar_t* a = p;
        const wchar_t* b = needle;
        while (*a && *b) {
            wchar_t ca = *a;
            wchar_t cb = *b;
            if (ca >= L'A' && ca <= L'Z') ca = static_cast<wchar_t>(ca - L'A' + L'a');
            if (cb >= L'A' && cb <= L'Z') cb = static_cast<wchar_t>(cb - L'A' + L'a');
            if (ca != cb) break;
            ++a;
            ++b;
        }
        if (!*b) return true;
    }
    return false;
}

void log_layouts() {
    logf("=== Windows SDK ARM64 ABI layout ===");
    logf("compile_target=_M_ARM64");
    logf("sizeof(void*)=%zu alignof(void*)=%zu", sizeof(void*), alignof(void*));
    logf("sizeof(HANDLE)=%zu alignof(HANDLE)=%zu", sizeof(HANDLE), alignof(HANDLE));
    logf("sizeof(GUID)=%zu alignof(GUID)=%zu", sizeof(GUID), alignof(GUID));

#define LOG_TYPE(T) logf("sizeof(%s)=%zu alignof(%s)=%zu", #T, sizeof(T), #T, alignof(T))
#define LOG_OFF(T, M) logf("offsetof(%s,%s)=%zu", #T, #M, offsetof(T, M))

    LOG_TYPE(SP_DEVICE_INTERFACE_DATA);
    LOG_OFF(SP_DEVICE_INTERFACE_DATA, cbSize);
    LOG_OFF(SP_DEVICE_INTERFACE_DATA, InterfaceClassGuid);
    LOG_OFF(SP_DEVICE_INTERFACE_DATA, Flags);
    LOG_OFF(SP_DEVICE_INTERFACE_DATA, Reserved);

    LOG_TYPE(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    LOG_OFF(SP_DEVICE_INTERFACE_DETAIL_DATA_W, cbSize);
    LOG_OFF(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath);

    LOG_TYPE(KSPROPERTY);
    LOG_OFF(KSPROPERTY, Set);
    LOG_OFF(KSPROPERTY, Id);
    LOG_OFF(KSPROPERTY, Flags);

    LOG_TYPE(KSPIN_INTERFACE);
    LOG_TYPE(KSPIN_MEDIUM);
    LOG_TYPE(KSPRIORITY);
    LOG_TYPE(KSPIN_CONNECT);
    LOG_OFF(KSPIN_CONNECT, Interface);
    LOG_OFF(KSPIN_CONNECT, Medium);
    LOG_OFF(KSPIN_CONNECT, PinId);
    LOG_OFF(KSPIN_CONNECT, PinToHandle);
    LOG_OFF(KSPIN_CONNECT, Priority);

    LOG_TYPE(KSDATAFORMAT);
    LOG_TYPE(WAVEFORMATEX);
    LOG_TYPE(WAVEFORMATEXTENSIBLE);
    LOG_TYPE(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    LOG_OFF(KSDATAFORMAT_WAVEFORMATEXTENSIBLE, DataFormat);
    LOG_OFF(KSDATAFORMAT_WAVEFORMATEXTENSIBLE, WaveFormatExt);

    LOG_TYPE(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION);
    LOG_OFF(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION, Property);
    LOG_OFF(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION, BaseAddress);
    LOG_OFF(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION, RequestedBufferSize);
    LOG_OFF(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION, NotificationCount);

    LOG_TYPE(KSRTAUDIO_BUFFER);
    LOG_OFF(KSRTAUDIO_BUFFER, BufferAddress);
    LOG_OFF(KSRTAUDIO_BUFFER, ActualBufferSize);
    LOG_OFF(KSRTAUDIO_BUFFER, CallMemoryBarrier);

    LOG_TYPE(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY);
    LOG_OFF(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY, Property);
    LOG_OFF(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY, NotificationEvent);

    LOG_TYPE(KSAUDIO_PRESENTATION_POSITION);
    LOG_OFF(KSAUDIO_PRESENTATION_POSITION, u64PositionInBlocks);
    LOG_OFF(KSAUDIO_PRESENTATION_POSITION, u64QPCPosition);

#undef LOG_TYPE
#undef LOG_OFF
}

void log_runtime_architecture() {
    USHORT process_machine = IMAGE_FILE_MACHINE_UNKNOWN;
    USHORT native_machine = IMAGE_FILE_MACHINE_UNKNOWN;
    if (IsWow64Process2(GetCurrentProcess(), &process_machine, &native_machine)) {
        logf("IsWow64Process2 processMachine=0x%04X nativeMachine=0x%04X",
             static_cast<unsigned>(process_machine),
             static_cast<unsigned>(native_machine));
    } else {
        logf("IsWow64Process2 failed Win32=%lu", GetLastError());
    }
}

bool find_x4_wave_path(wchar_t* output, size_t output_chars) {
    HDEVINFO set = SetupDiGetClassDevsW(
        &KSCATEGORY_AUDIO,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (set == INVALID_HANDLE_VALUE) {
        logf("SetupDiGetClassDevsW failed Win32=%lu", GetLastError());
        return false;
    }

    bool found = false;
    for (DWORD index = 0; ; ++index) {
        SP_DEVICE_INTERFACE_DATA interface_data{};
        interface_data.cbSize = sizeof(interface_data);

        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &KSCATEGORY_AUDIO, index, &interface_data)) {
            const DWORD error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) break;
            logf("SetupDiEnumDeviceInterfaces index=%lu failed Win32=%lu", index, error);
            continue;
        }

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &interface_data, nullptr, 0, &required, nullptr);
        if (required == 0 || required > 8192) {
            logf("SetupDiGetDeviceInterfaceDetailW size index=%lu unexpected required=%lu", index, required);
            continue;
        }

        alignas(SP_DEVICE_INTERFACE_DETAIL_DATA_W) BYTE detail_storage[8192]{};
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detail_storage);
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(
                set,
                &interface_data,
                detail,
                static_cast<DWORD>(sizeof(detail_storage)),
                &required,
                nullptr)) {
            logf("SetupDiGetDeviceInterfaceDetailW index=%lu failed Win32=%lu", index, GetLastError());
            continue;
        }

        if (contains_ascii_i(detail->DevicePath, L"vid_041e&pid_3278&mi_03") &&
            contains_ascii_i(detail->DevicePath, L"\\msft_wave")) {
            if (wcslen(detail->DevicePath) + 1 > output_chars) {
                logf("X4 msft_wave path is longer than output buffer");
                break;
            }
            wcscpy_s(output, output_chars, detail->DevicePath);
            found = true;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(set);
    return found;
}

struct PinCreateRequest {
    KSPIN_CONNECT Connect;
    KSDATAFORMAT_WAVEFORMATEXTENSIBLE Format;
};

static_assert(offsetof(PinCreateRequest, Format) == sizeof(KSPIN_CONNECT),
              "KSDATAFORMAT must immediately follow KSPIN_CONNECT");
static_assert(sizeof(void*) == 8, "Native ARM64 must use 64-bit pointers");

PinCreateRequest make_pin_request() {
    PinCreateRequest request{};

    request.Connect.Interface.Set = KSINTERFACESETID_Standard;
    request.Connect.Interface.Id = KSINTERFACE_STANDARD_LOOPED_STREAMING;
    request.Connect.Interface.Flags = 0;

    request.Connect.Medium.Set = KSMEDIUMSETID_Standard;
    request.Connect.Medium.Id = KSMEDIUM_STANDARD_DEVIO;
    request.Connect.Medium.Flags = 0;

    request.Connect.PinId = kRenderPinId;
    request.Connect.PinToHandle = nullptr;
    request.Connect.Priority.PriorityClass = KSPRIORITY_NORMAL;
    request.Connect.Priority.PrioritySubClass = 1;

    request.Format.DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    request.Format.DataFormat.Flags = 0;
    request.Format.DataFormat.SampleSize = 0; // Preserve the hardware-confirmed A0 request.
    request.Format.DataFormat.Reserved = 0;
    request.Format.DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    request.Format.DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    request.Format.DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;

    auto& wave = request.Format.WaveFormatExt;
    wave.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wave.Format.nChannels = kChannels;
    wave.Format.nSamplesPerSec = kSampleRate;
    wave.Format.nAvgBytesPerSec = kSampleRate * kBytesPerFrame;
    wave.Format.nBlockAlign = static_cast<WORD>(kBytesPerFrame);
    wave.Format.wBitsPerSample = kBitsPerSample;
    wave.Format.cbSize = static_cast<WORD>(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX));
    wave.Samples.wValidBitsPerSample = kBitsPerSample;
    wave.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wave.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    return request;
}

KSPROPERTY make_property(const GUID& set, ULONG id, ULONG flags) {
    KSPROPERTY property{};
    property.Set = set;
    property.Id = id;
    property.Flags = flags;
    return property;
}

bool set_state(HANDLE pin, KSSTATE state) {
    KSPROPERTY property = make_property(
        KSPROPSETID_Connection,
        KSPROPERTY_CONNECTION_STATE,
        KSPROPERTY_TYPE_SET);

    DWORD returned = 0;
    if (!DeviceIoControl(
            pin,
            IOCTL_KS_PROPERTY,
            &property,
            sizeof(property),
            &state,
            sizeof(state),
            &returned,
            nullptr)) {
        logf("KSPROPERTY_CONNECTION_STATE state=%u failed Win32=%lu",
             static_cast<unsigned>(state), GetLastError());
        return false;
    }

    logf("KSSTATE %u -> OK", static_cast<unsigned>(state));
    return true;
}

bool get_packet_count(HANDLE pin, ULONG* packet_count) {
    KSPROPERTY property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_PACKETCOUNT,
        KSPROPERTY_TYPE_GET);

    DWORD returned = 0;
    return !!DeviceIoControl(
        pin,
        IOCTL_KS_PROPERTY,
        &property,
        sizeof(property),
        packet_count,
        sizeof(*packet_count),
        &returned,
        nullptr);
}

bool get_presentation_position(HANDLE pin, KSAUDIO_PRESENTATION_POSITION* position) {
    KSPROPERTY property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_PRESENTATION_POSITION,
        KSPROPERTY_TYPE_GET);

    DWORD returned = 0;
    return !!DeviceIoControl(
        pin,
        IOCTL_KS_PROPERTY,
        &property,
        sizeof(property),
        position,
        sizeof(*position),
        &returned,
        nullptr);
}

struct RunStats {
    ULONG notifications = 0;
    ULONG packet_discontinuities = 0;
    ULONG position_regressions = 0;
};

bool run_single_a0_lifecycle(RunStats* stats) {
    wchar_t path[1024]{};
    if (!find_x4_wave_path(path, _countof(path))) {
        logf("X4 msft_wave filter not found");
        return false;
    }

    logf("A0-SDK checkpoint: opening msft_wave filter");
    HANDLE filter = CreateFileW(
        path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (filter == INVALID_HANDLE_VALUE) {
        logf("CreateFileW(msft_wave) failed Win32=%lu", GetLastError());
        return false;
    }

    logf("A0-SDK checkpoint: creating Render Pin 1 with SDK KSPIN_CONNECT/KSDATAFORMAT_WAVEFORMATEXTENSIBLE");
    PinCreateRequest pin_request = make_pin_request();
    HANDLE pin = nullptr;
    const DWORD status = KsCreatePin(filter, &pin_request.Connect, GENERIC_WRITE, &pin);
    if (status != 0 || !pin || pin == INVALID_HANDLE_VALUE) {
        logf("KsCreatePin failed status=0x%08lX", static_cast<unsigned long>(status));
        CloseHandle(filter);
        return false;
    }

    logf("A0-SDK checkpoint: requesting WaveRT notification buffer");
    KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION request{};
    request.Property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION,
        KSPROPERTY_TYPE_GET);
    request.BaseAddress = nullptr;
    request.RequestedBufferSize = kRequestedBufferBytes;
    request.NotificationCount = kNotificationCount;

    KSRTAUDIO_BUFFER buffer{};
    DWORD returned = 0;
    if (!DeviceIoControl(
            pin,
            IOCTL_KS_PROPERTY,
            &request,
            sizeof(request),
            &buffer,
            sizeof(buffer),
            &returned,
            nullptr)) {
        logf("BUFFER_WITH_NOTIFICATION failed Win32=%lu", GetLastError());
        CloseHandle(pin);
        CloseHandle(filter);
        return false;
    }

    logf("WaveRT BufferAddress=%p ActualBufferSize=%lu CallMemoryBarrier=%d",
         buffer.BufferAddress,
         buffer.ActualBufferSize,
         buffer.CallMemoryBarrier ? 1 : 0);

    if (!buffer.BufferAddress || buffer.ActualBufferSize != kRequestedBufferBytes) {
        logf("Unexpected WaveRT buffer geometry");
        CloseHandle(pin);
        CloseHandle(filter);
        return false;
    }

    logf("A0-SDK checkpoint: zeroing entire buffer once before RUN");
    ZeroMemory(buffer.BufferAddress, buffer.ActualBufferSize);
    if (buffer.CallMemoryBarrier) {
        MemoryBarrier();
        logf("A0-SDK checkpoint: MemoryBarrier() issued because SDK KSRTAUDIO_BUFFER requested it");
    }

    HANDLE event_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_handle) {
        logf("CreateEventW failed Win32=%lu", GetLastError());
        CloseHandle(pin);
        CloseHandle(filter);
        return false;
    }

    logf("A0-SDK checkpoint: registering notification event");
    KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY notification{};
    notification.Property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT,
        KSPROPERTY_TYPE_SET);
    notification.NotificationEvent = event_handle;

    if (!DeviceIoControl(
            pin,
            IOCTL_KS_PROPERTY,
            &notification,
            sizeof(notification),
            nullptr,
            0,
            &returned,
            nullptr)) {
        logf("REGISTER_NOTIFICATION_EVENT failed Win32=%lu", GetLastError());
        CloseHandle(event_handle);
        CloseHandle(pin);
        CloseHandle(filter);
        return false;
    }

    bool entered_run = false;
    bool ok = false;

    logf("A0-SDK checkpoint: KSSTATE_ACQUIRE");
    if (!set_state(pin, KSSTATE_ACQUIRE)) goto cleanup;
    logf("A0-SDK checkpoint: KSSTATE_PAUSE");
    if (!set_state(pin, KSSTATE_PAUSE)) goto cleanup;
    logf("A0-SDK checkpoint: KSSTATE_RUN");
    if (!set_state(pin, KSSTATE_RUN)) goto cleanup;
    entered_run = true;

    logf("A0-SDK checkpoint: RUN entered; observing 20 notifications only");
    {
        ULONG previous_packet = 0;
        UINT64 previous_position = 0;
        bool have_previous_position = false;

        for (ULONG i = 0; i < kNotificationsPerRun; ++i) {
            logf("A0-SDK checkpoint: wait notification %lu", i + 1);
            const DWORD wait = WaitForSingleObject(event_handle, 250);
            if (wait != WAIT_OBJECT_0) {
                if (wait == WAIT_TIMEOUT) {
                    logf("DMA notification timeout");
                } else {
                    logf("WaitForSingleObject failed result=%lu Win32=%lu", wait, GetLastError());
                }
                goto cleanup;
            }

            ULONG packet_count = 0;
            if (!get_packet_count(pin, &packet_count)) {
                logf("PACKETCOUNT failed Win32=%lu", GetLastError());
                goto cleanup;
            }

            KSAUDIO_PRESENTATION_POSITION position{};
            if (!get_presentation_position(pin, &position)) {
                logf("PRESENTATION_POSITION failed Win32=%lu", GetLastError());
                goto cleanup;
            }

            if (previous_packet && packet_count != previous_packet + 1) {
                ++stats->packet_discontinuities;
            }
            previous_packet = packet_count;

            if (have_previous_position && position.u64PositionInBlocks < previous_position) {
                ++stats->position_regressions;
            }
            previous_position = position.u64PositionInBlocks;
            have_previous_position = true;

            ++stats->notifications;
            logf("notification=%lu packet=%lu samplePosition=%llu qpc=%llu",
                 i + 1,
                 packet_count,
                 static_cast<unsigned long long>(position.u64PositionInBlocks),
                 static_cast<unsigned long long>(position.u64QPCPosition));
        }
    }

    ok = true;

cleanup:
    logf("A0-SDK checkpoint: cleanup begin");
    if (entered_run) {
        logf("A0-SDK checkpoint: RUN->PAUSE");
        set_state(pin, KSSTATE_PAUSE);
        logf("A0-SDK checkpoint: PAUSE->ACQUIRE");
        set_state(pin, KSSTATE_ACQUIRE);
        logf("A0-SDK checkpoint: ACQUIRE->STOP");
        set_state(pin, KSSTATE_STOP);
    }

    logf("A0-SDK checkpoint: unregister notification event");
    notification.Property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT,
        KSPROPERTY_TYPE_SET);
    if (!DeviceIoControl(
            pin,
            IOCTL_KS_PROPERTY,
            &notification,
            sizeof(notification),
            nullptr,
            0,
            &returned,
            nullptr)) {
        logf("UNREGISTER_NOTIFICATION_EVENT failed Win32=%lu", GetLastError());
    } else {
        logf("A0-SDK checkpoint: unregister OK");
    }

    CloseHandle(event_handle);
    CloseHandle(pin);
    CloseHandle(filter);
    logf("A0-SDK checkpoint: clean close complete");
    return ok;
}

} // namespace

int main() {
    if (fopen_s(&g_log, "x4-asio-sdk-abi-baseline.txt", "wb") != 0) {
        g_log = nullptr;
    }

    logf("Sound Blaster X4 Windows ARM64 - Windows SDK ABI Baseline A0");
    logf("Scope: official Windows SDK types only; one open/RUN/STOP/close lifecycle; 20 notifications; no writes during RUN");
    logf("Creative runtime dependencies: NONE");

    log_runtime_architecture();
    log_layouts();

    RunStats stats{};
    const bool runtime_ok = run_single_a0_lifecycle(&stats);

    logf("notifications=%lu", stats.notifications);
    logf("packet_discontinuities=%lu", stats.packet_discontinuities);
    logf("position_regressions=%lu", stats.position_regressions);

    const bool pass = runtime_ok &&
                      stats.notifications == kNotificationsPerRun &&
                      stats.packet_discontinuities == 0 &&
                      stats.position_regressions == 0;

    logf(pass ? "SDK ABI BASELINE RESULT: PASS" : "SDK ABI BASELINE RESULT: FAIL");

    if (g_log) {
        std::fclose(g_log);
        g_log = nullptr;
    }

    return pass ? 0 : 2;
}
