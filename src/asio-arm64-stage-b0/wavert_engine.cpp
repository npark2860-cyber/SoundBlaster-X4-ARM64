#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <ks.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <ksproxy.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "wavert_engine.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error Stage B3A WaveRT engine must be built for native Windows ARM64, not ARM64EC.
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

bool find_x4_wave_path(wchar_t* output, size_t output_chars) {
    HDEVINFO set = SetupDiGetClassDevsW(
        &KSCATEGORY_AUDIO,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (set == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    for (DWORD index = 0; ; ++index) {
        SP_DEVICE_INTERFACE_DATA interface_data{};
        interface_data.cbSize = sizeof(interface_data);

        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &KSCATEGORY_AUDIO, index, &interface_data)) {
            const DWORD error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) break;
            continue;
        }

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &interface_data, nullptr, 0, &required, nullptr);
        if (required == 0 || required > 8192) continue;

        alignas(SP_DEVICE_INTERFACE_DETAIL_DATA_W) BYTE storage[8192]{};
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage);
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(
                set,
                &interface_data,
                detail,
                static_cast<DWORD>(sizeof(storage)),
                &required,
                nullptr)) {
            continue;
        }

        if (contains_ascii_i(detail->DevicePath, L"vid_041e&pid_3278&mi_03") &&
            contains_ascii_i(detail->DevicePath, L"\\msft_wave")) {
            if (wcslen(detail->DevicePath) + 1 <= output_chars) {
                wcscpy_s(output, output_chars, detail->DevicePath);
                found = true;
            }
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(set);
    return found;
}

bool query_instances(
    HANDLE filter,
    ULONG property_id,
    ULONG* possible,
    ULONG* current,
    DWORD* error_out) {

    KSP_PIN request{};
    request.Property.Set = KSPROPSETID_Pin;
    request.Property.Id = property_id;
    request.Property.Flags = KSPROPERTY_TYPE_GET;
    request.PinId = kRenderPinId;
    request.Reserved = 0;

    KSPIN_CINSTANCES instances{};
    DWORD returned = 0;
    if (!DeviceIoControl(
            filter,
            IOCTL_KS_PROPERTY,
            &request,
            sizeof(request),
            &instances,
            sizeof(instances),
            &returned,
            nullptr)) {
        if (error_out) *error_out = GetLastError();
        return false;
    }

    if (possible) *possible = instances.PossibleCount;
    if (current) *current = instances.CurrentCount;
    if (error_out) *error_out = ERROR_SUCCESS;
    return true;
}

struct PinCreateRequest {
    KSPIN_CONNECT Connect;
    KSDATAFORMAT_WAVEFORMATEXTENSIBLE Format;
};

static_assert(offsetof(PinCreateRequest, Format) == sizeof(KSPIN_CONNECT),
              "KSDATAFORMAT must immediately follow KSPIN_CONNECT");

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
    request.Format.DataFormat.SampleSize = 0;
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
    const BOOL ok = DeviceIoControl(
        pin,
        IOCTL_KS_PROPERTY,
        &property,
        sizeof(property),
        &state,
        sizeof(state),
        &returned,
        nullptr);

    std::printf("B2 KSSTATE %u -> %s\n", static_cast<unsigned>(state), ok ? "OK" : "FAIL");
    return !!ok;
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

} // namespace

X4WaveRtEngine::~X4WaveRtEngine() {
    dispose();
}

X4WaveRtPrepareResult X4WaveRtEngine::prepare() {
    dispose();
    stats_ = {};

    wchar_t path[1024]{};
    if (!find_x4_wave_path(path, sizeof(path) / sizeof(path[0]))) {
        strcpy_s(last_message_, "Stage B2 prepare FAILED: X4 msft_wave not found");
        return X4WaveRtPrepareResult::Failed;
    }

    filter_ = CreateFileW(
        path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (filter_ == INVALID_HANDLE_VALUE) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 prepare FAILED: filter open Win32=%lu", GetLastError());
        return X4WaveRtPrepareResult::Failed;
    }

    ULONG local_possible = 0;
    ULONG local_current = 0;
    ULONG global_possible = 0;
    ULONG global_current = 0;
    DWORD local_error = ERROR_SUCCESS;
    DWORD global_error = ERROR_SUCCESS;

    const bool local_ok = query_instances(
        filter_,
        static_cast<ULONG>(KSPROPERTY_PIN_CINSTANCES),
        &local_possible,
        &local_current,
        &local_error);

    const bool global_ok = query_instances(
        filter_,
        static_cast<ULONG>(KSPROPERTY_PIN_GLOBALCINSTANCES),
        &global_possible,
        &global_current,
        &global_error);

    if (!local_ok || !global_ok) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 PRE-PIN gate INDETERMINATE: C ok=%d err=%lu G ok=%d err=%lu; KsCreatePin SKIPPED",
                  local_ok ? 1 : 0, local_error, global_ok ? 1 : 0, global_error);
        CloseHandle(filter_);
        filter_ = INVALID_HANDLE_VALUE;
        return X4WaveRtPrepareResult::Indeterminate;
    }

    const bool busy =
        local_current >= local_possible ||
        global_current >= global_possible;

    std::printf(
        "B2 PRE-PIN GATE: C %lu/%lu G %lu/%lu busy=%s\n",
        local_current,
        local_possible,
        global_current,
        global_possible,
        busy ? "YES" : "NO");

    if (busy) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 PRE-PIN gate BUSY: C %lu/%lu G %lu/%lu; KsCreatePin SKIPPED",
                  local_current, local_possible, global_current, global_possible);
        CloseHandle(filter_);
        filter_ = INVALID_HANDLE_VALUE;
        return X4WaveRtPrepareResult::Busy;
    }

    PinCreateRequest pin_request = make_pin_request();
    HANDLE created_pin = nullptr;
    const DWORD status = KsCreatePin(filter_, &pin_request.Connect, GENERIC_WRITE, &created_pin);
    if (status != ERROR_SUCCESS || !created_pin || created_pin == INVALID_HANDLE_VALUE) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 KsCreatePin FAILED status=0x%08lX",
                  static_cast<unsigned long>(status));
        CloseHandle(filter_);
        filter_ = INVALID_HANDLE_VALUE;
        return X4WaveRtPrepareResult::Failed;
    }
    pin_ = created_pin;

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
            pin_,
            IOCTL_KS_PROPERTY,
            &request,
            sizeof(request),
            &buffer,
            sizeof(buffer),
            &returned,
            nullptr)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 BUFFER_WITH_NOTIFICATION FAILED Win32=%lu", GetLastError());
        dispose();
        return X4WaveRtPrepareResult::Failed;
    }

    if (!buffer.BufferAddress || buffer.ActualBufferSize != kRequestedBufferBytes) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 unexpected WaveRT buffer address=%p size=%lu",
                  buffer.BufferAddress,
                  buffer.ActualBufferSize);
        dispose();
        return X4WaveRtPrepareResult::Failed;
    }

    std::printf(
        "B2 WaveRT BufferAddress=%p ActualBufferSize=%lu CallMemoryBarrier=%d\n",
        buffer.BufferAddress,
        buffer.ActualBufferSize,
        buffer.CallMemoryBarrier ? 1 : 0);

    ZeroMemory(buffer.BufferAddress, buffer.ActualBufferSize);
    if (buffer.CallMemoryBarrier) MemoryBarrier();

    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 CreateEventW FAILED Win32=%lu", GetLastError());
        dispose();
        return X4WaveRtPrepareResult::Failed;
    }

    KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY notification{};
    notification.Property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT,
        KSPROPERTY_TYPE_SET);
    notification.NotificationEvent = event_;

    if (!DeviceIoControl(
            pin_,
            IOCTL_KS_PROPERTY,
            &notification,
            sizeof(notification),
            nullptr,
            0,
            &returned,
            nullptr)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 REGISTER_NOTIFICATION_EVENT FAILED Win32=%lu", GetLastError());
        dispose();
        return X4WaveRtPrepareResult::Failed;
    }

    notification_registered_ = true;
    prepared_ = true;
    strcpy_s(last_message_, "Stage B2 WaveRT prepared: pin/buffer/event ready; not RUN yet");
    return X4WaveRtPrepareResult::Ready;
}

bool X4WaveRtEngine::start_and_observe(
    X4WaveRtNotificationObserver observer,
    void* observer_context) {

    if (!prepared_ || pin_ == INVALID_HANDLE_VALUE || !event_) {
        strcpy_s(last_message_, "Stage B2 start FAILED: engine not prepared");
        return false;
    }
    if (entered_run_) {
        strcpy_s(last_message_, "Stage B2 start FAILED: already RUN");
        return false;
    }

    stats_ = {};

    if (!set_state(pin_, KSSTATE_ACQUIRE)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 ACQUIRE FAILED Win32=%lu", GetLastError());
        return false;
    }
    if (!set_state(pin_, KSSTATE_PAUSE)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 PAUSE FAILED Win32=%lu", GetLastError());
        return false;
    }
    if (!set_state(pin_, KSSTATE_RUN)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 RUN FAILED Win32=%lu", GetLastError());
        return false;
    }
    entered_run_ = true;

    ULONG previous_packet = 0;
    UINT64 previous_position = 0;
    bool have_previous_position = false;

    for (ULONG i = 0; i < kNotificationsPerRun; ++i) {
        const DWORD wait = WaitForSingleObject(event_, 250);
        if (wait != WAIT_OBJECT_0) {
            if (wait == WAIT_TIMEOUT) {
                strcpy_s(last_message_, "Stage B2 RUN FAILED: DMA notification timeout");
            } else {
                sprintf_s(last_message_, sizeof(last_message_),
                          "Stage B2 RUN FAILED: wait=%lu Win32=%lu", wait, GetLastError());
            }
            stop();
            return false;
        }

        ULONG packet_count = 0;
        if (!get_packet_count(pin_, &packet_count)) {
            sprintf_s(last_message_, sizeof(last_message_),
                      "Stage B2 PACKETCOUNT FAILED Win32=%lu", GetLastError());
            stop();
            return false;
        }

        KSAUDIO_PRESENTATION_POSITION position{};
        if (!get_presentation_position(pin_, &position)) {
            sprintf_s(last_message_, sizeof(last_message_),
                      "Stage B2 PRESENTATION_POSITION FAILED Win32=%lu", GetLastError());
            stop();
            return false;
        }

        if (previous_packet && packet_count != previous_packet + 1) {
            ++stats_.packet_discontinuities;
        }
        previous_packet = packet_count;

        if (have_previous_position && position.u64PositionInBlocks < previous_position) {
            ++stats_.position_regressions;
        }
        previous_position = position.u64PositionInBlocks;
        have_previous_position = true;

        ++stats_.notifications;
        std::printf(
            "B2 notification=%lu packet=%lu samplePosition=%llu qpc=%llu\n",
            i + 1,
            packet_count,
            static_cast<unsigned long long>(position.u64PositionInBlocks),
            static_cast<unsigned long long>(position.u64QPCPosition));

        // Stage B3A adds only a host-facing observer after the B2 notification
        // has already passed packet/position validation. The observer is not
        // given the WaveRT buffer and cannot write hardware DMA memory.
        if (observer) {
            observer(observer_context, i);
        }
    }

    if (stats_.packet_discontinuities != 0 || stats_.position_regressions != 0) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 RUN quality FAILED: notifications=%lu packetDiscontinuities=%lu positionRegressions=%lu",
                  stats_.notifications,
                  stats_.packet_discontinuities,
                  stats_.position_regressions);
        stop();
        return false;
    }

    sprintf_s(last_message_, sizeof(last_message_),
              "Stage B2 RUN observed %lu/20 notifications; packetDiscontinuities=0 positionRegressions=0",
              stats_.notifications);
    return true;
}

bool X4WaveRtEngine::stop() {
    if (!entered_run_) return true;

    bool ok = true;
    if (!set_state(pin_, KSSTATE_PAUSE)) ok = false;
    if (!set_state(pin_, KSSTATE_ACQUIRE)) ok = false;
    if (!set_state(pin_, KSSTATE_STOP)) ok = false;
    entered_run_ = false;

    if (!ok) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B2 stop FAILED Win32=%lu", GetLastError());
    }
    return ok;
}

void X4WaveRtEngine::dispose() {
    if (entered_run_) {
        stop();
    }

    if (notification_registered_ && pin_ != INVALID_HANDLE_VALUE && event_) {
        KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY notification{};
        notification.Property = make_property(
            KSPROPSETID_RtAudio,
            KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT,
            KSPROPERTY_TYPE_SET);
        notification.NotificationEvent = event_;
        DWORD returned = 0;
        if (DeviceIoControl(
                pin_,
                IOCTL_KS_PROPERTY,
                &notification,
                sizeof(notification),
                nullptr,
                0,
                &returned,
                nullptr)) {
            std::printf("B2 unregister notification -> OK\n");
        } else {
            std::printf("B2 unregister notification -> FAIL Win32=%lu\n", GetLastError());
        }
    }

    notification_registered_ = false;

    if (event_) {
        CloseHandle(event_);
        event_ = nullptr;
    }
    if (pin_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pin_);
        pin_ = INVALID_HANDLE_VALUE;
    }
    if (filter_ != INVALID_HANDLE_VALUE) {
        CloseHandle(filter_);
        filter_ = INVALID_HANDLE_VALUE;
    }

    prepared_ = false;
    entered_run_ = false;
}
