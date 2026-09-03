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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "wavert_engine.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error Stage B4A WaveRT engine must be built for native Windows ARM64, not ARM64EC.
#endif

namespace {

constexpr ULONG kRenderPinId = 1;
constexpr ULONG kSampleRate = 48000;
constexpr WORD  kChannels = 2;
constexpr WORD  kBitsPerSample = 16;
constexpr ULONG kBytesPerFrame = 4;
constexpr ULONG kRequestedBufferBytes = 4096;
constexpr ULONG kNotificationCount = 2;
constexpr ULONG kPacketFrames = 512;
constexpr ULONG kPacketBytes = kPacketFrames * kBytesPerFrame;

static_assert(kPacketBytes * kNotificationCount == kRequestedBufferBytes,
              "B4A packet geometry must exactly cover the WaveRT cyclic buffer");

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
    request.Connect.Medium.Set = KSMEDIUMSETID_Standard;
    request.Connect.Medium.Id = KSMEDIUM_STANDARD_DEVIO;
    request.Connect.PinId = kRenderPinId;
    request.Connect.PinToHandle = nullptr;
    request.Connect.Priority.PriorityClass = KSPRIORITY_NORMAL;
    request.Connect.Priority.PrioritySubClass = 1;

    request.Format.DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
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
    std::printf("B4A KSSTATE %u -> %s\n", static_cast<unsigned>(state), ok ? "OK" : "FAIL");
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
    previous_packet_ = 0;
    previous_position_ = 0;
    have_previous_position_ = false;

    wchar_t path[1024]{};
    if (!find_x4_wave_path(path, sizeof(path) / sizeof(path[0]))) {
        strcpy_s(last_message_, "Stage B4A prepare FAILED: X4 msft_wave not found");
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
                  "Stage B4A prepare FAILED: filter open Win32=%lu", GetLastError());
        return X4WaveRtPrepareResult::Failed;
    }

    ULONG local_possible = 0;
    ULONG local_current = 0;
    ULONG global_possible = 0;
    ULONG global_current = 0;
    DWORD local_error = ERROR_SUCCESS;
    DWORD global_error = ERROR_SUCCESS;

    const bool local_ok = query_instances(
        filter_, KSPROPERTY_PIN_CINSTANCES,
        &local_possible, &local_current, &local_error);
    const bool global_ok = query_instances(
        filter_, KSPROPERTY_PIN_GLOBALCINSTANCES,
        &global_possible, &global_current, &global_error);

    if (!local_ok || !global_ok) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B4A PRE-PIN gate INDETERMINATE: C ok=%d err=%lu G ok=%d err=%lu; KsCreatePin SKIPPED",
                  local_ok ? 1 : 0, local_error, global_ok ? 1 : 0, global_error);
        CloseHandle(filter_);
        filter_ = INVALID_HANDLE_VALUE;
        return X4WaveRtPrepareResult::Indeterminate;
    }

    const bool busy =
        local_current >= local_possible || global_current >= global_possible;
    std::printf(
        "B4A PRE-PIN GATE: C %lu/%lu G %lu/%lu busy=%s\n",
        local_current, local_possible, global_current, global_possible,
        busy ? "YES" : "NO");

    if (busy) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B4A PRE-PIN gate BUSY: C %lu/%lu G %lu/%lu; KsCreatePin SKIPPED",
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
                  "Stage B4A KsCreatePin FAILED status=0x%08lX",
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
            pin_, IOCTL_KS_PROPERTY,
            &request, sizeof(request),
            &buffer, sizeof(buffer),
            &returned, nullptr)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B4A BUFFER_WITH_NOTIFICATION FAILED Win32=%lu", GetLastError());
        dispose();
        return X4WaveRtPrepareResult::Failed;
    }

    if (!buffer.BufferAddress || buffer.ActualBufferSize != kRequestedBufferBytes) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B4A unexpected WaveRT buffer address=%p size=%lu",
                  buffer.BufferAddress, buffer.ActualBufferSize);
        dispose();
        return X4WaveRtPrepareResult::Failed;
    }

    buffer_address_ = buffer.BufferAddress;
    actual_buffer_size_ = buffer.ActualBufferSize;
    call_memory_barrier_ = !!buffer.CallMemoryBarrier;

    std::printf(
        "B4A WaveRT BufferAddress=%p ActualBufferSize=%lu CallMemoryBarrier=%d packetBytes=%lu\n",
        buffer_address_, actual_buffer_size_, call_memory_barrier_ ? 1 : 0, kPacketBytes);

    ZeroMemory(buffer_address_, actual_buffer_size_);
    if (call_memory_barrier_) MemoryBarrier();

    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B4A CreateEventW FAILED Win32=%lu", GetLastError());
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
            pin_, IOCTL_KS_PROPERTY,
            &notification, sizeof(notification),
            nullptr, 0, &returned, nullptr)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B4A REGISTER_NOTIFICATION_EVENT FAILED Win32=%lu", GetLastError());
        dispose();
        return X4WaveRtPrepareResult::Failed;
    }

    notification_registered_ = true;
    prepared_ = true;
    strcpy_s(last_message_, "Stage B4A WaveRT prepared: mapped 2x2048-byte packets; not RUN yet");
    return X4WaveRtPrepareResult::Ready;
}

bool X4WaveRtEngine::write_interleaved_packet(
    ULONG absolute_packet_number,
    const std::int16_t* left,
    const std::int16_t* right,
    ULONG frames) {

    if (!prepared_ || !buffer_address_ || actual_buffer_size_ != kRequestedBufferBytes) {
        strcpy_s(last_message_, "Stage B4A DMA copy FAILED: WaveRT buffer not prepared");
        return false;
    }
    if (!left || !right || frames != kPacketFrames) {
        strcpy_s(last_message_, "Stage B4A DMA copy FAILED: expected two non-null planar int16 x 512 buffers");
        return false;
    }

    const ULONG slot = absolute_packet_number % kNotificationCount;
    const ULONG byte_offset = slot * kPacketBytes;
    if (byte_offset + kPacketBytes > actual_buffer_size_) {
        strcpy_s(last_message_, "Stage B4A DMA copy FAILED: packet offset out of range");
        return false;
    }

    auto* target = reinterpret_cast<std::int16_t*>(
        static_cast<BYTE*>(buffer_address_) + byte_offset);

    ULONG nonzero = 0;
    for (ULONG frame = 0; frame < frames; ++frame) {
        const std::int16_t l = left[frame];
        const std::int16_t r = right[frame];
        target[frame * 2] = l;
        target[frame * 2 + 1] = r;
        if (l != 0) ++nonzero;
        if (r != 0) ++nonzero;
    }

    if (call_memory_barrier_) MemoryBarrier();

    ++stats_.hardware_buffer_writes;
    stats_.dma_frames_copied += frames;
    stats_.dma_nonzero_samples += nonzero;
    stats_.last_write_packet = absolute_packet_number;

    std::printf(
        "B4A DMA writePacket=%lu slot=%lu frames=%lu nonzeroSamples=%lu\n",
        absolute_packet_number, slot, frames, nonzero);
    return true;
}

bool X4WaveRtEngine::start_run() {
    if (!prepared_ || pin_ == INVALID_HANDLE_VALUE || !event_) {
        strcpy_s(last_message_, "Stage B4A start FAILED: engine not prepared");
        return false;
    }
    if (entered_run_) {
        strcpy_s(last_message_, "Stage B4A start FAILED: already RUN");
        return false;
    }

    stats_ = {};
    previous_packet_ = 0;
    previous_position_ = 0;
    have_previous_position_ = false;

    if (!set_state(pin_, KSSTATE_ACQUIRE) ||
        !set_state(pin_, KSSTATE_PAUSE) ||
        !set_state(pin_, KSSTATE_RUN)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B4A state transition FAILED Win32=%lu", GetLastError());
        return false;
    }

    entered_run_ = true;
    strcpy_s(last_message_, "Stage B4A RUN entered; notification worker may start");
    return true;
}

X4WaveRtProcessResult X4WaveRtEngine::process_one_notification(
    HANDLE stop_event,
    X4WaveRtNotificationObserver observer,
    void* observer_context) {

    if (!entered_run_ || !event_ || pin_ == INVALID_HANDLE_VALUE) {
        strcpy_s(last_message_, "Stage B4A process FAILED: engine not RUN");
        return X4WaveRtProcessResult::Failed;
    }
    if (!stop_event) {
        strcpy_s(last_message_, "Stage B4A process FAILED: stop event missing");
        return X4WaveRtProcessResult::Failed;
    }

    HANDLE handles[2] = {stop_event, event_};
    const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, 250);
    if (wait == WAIT_OBJECT_0) {
        return X4WaveRtProcessResult::StopRequested;
    }
    if (wait != WAIT_OBJECT_0 + 1) {
        if (wait == WAIT_TIMEOUT) {
            strcpy_s(last_message_, "Stage B4A worker FAILED: DMA notification timeout");
        } else {
            sprintf_s(last_message_, sizeof(last_message_),
                      "Stage B4A worker FAILED: wait=%lu Win32=%lu", wait, GetLastError());
        }
        return X4WaveRtProcessResult::Failed;
    }

    ULONG packet_count = 0;
    if (!get_packet_count(pin_, &packet_count)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B4A PACKETCOUNT FAILED Win32=%lu", GetLastError());
        return X4WaveRtProcessResult::Failed;
    }

    KSAUDIO_PRESENTATION_POSITION position{};
    if (!get_presentation_position(pin_, &position)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "Stage B4A PRESENTATION_POSITION FAILED Win32=%lu", GetLastError());
        return X4WaveRtProcessResult::Failed;
    }

    if (previous_packet_ && packet_count != previous_packet_ + 1) {
        ++stats_.packet_discontinuities;
    }
    previous_packet_ = packet_count;

    if (have_previous_position_ && position.u64PositionInBlocks < previous_position_) {
        ++stats_.position_regressions;
    }
    previous_position_ = position.u64PositionInBlocks;
    have_previous_position_ = true;

    const ULONG notification_index = stats_.notifications;
    ++stats_.notifications;
    const ULONG write_packet = packet_count + 1;
    const ULONG write_slot = write_packet % kNotificationCount;

    std::printf(
        "B4A notification=%lu packet=%lu writePacket=%lu slot=%lu samplePosition=%llu qpc=%llu thread=%lu\n",
        stats_.notifications,
        packet_count,
        write_packet,
        write_slot,
        static_cast<unsigned long long>(position.u64PositionInBlocks),
        static_cast<unsigned long long>(position.u64QPCPosition),
        GetCurrentThreadId());

    if (observer && !observer(observer_context, notification_index, packet_count)) {
        return X4WaveRtProcessResult::Failed;
    }

    return X4WaveRtProcessResult::Notification;
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
                  "Stage B4A stop FAILED Win32=%lu", GetLastError());
    }
    return ok;
}

void X4WaveRtEngine::dispose() {
    if (entered_run_) stop();

    if (notification_registered_ && pin_ != INVALID_HANDLE_VALUE && event_) {
        KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY notification{};
        notification.Property = make_property(
            KSPROPSETID_RtAudio,
            KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT,
            KSPROPERTY_TYPE_SET);
        notification.NotificationEvent = event_;
        DWORD returned = 0;
        if (DeviceIoControl(
                pin_, IOCTL_KS_PROPERTY,
                &notification, sizeof(notification),
                nullptr, 0, &returned, nullptr)) {
            std::printf("B4A unregister notification -> OK\n");
        } else {
            std::printf("B4A unregister notification -> FAIL Win32=%lu\n", GetLastError());
        }
    }

    notification_registered_ = false;
    buffer_address_ = nullptr;
    actual_buffer_size_ = 0;
    call_memory_barrier_ = false;
    previous_packet_ = 0;
    previous_position_ = 0;
    have_previous_position_ = false;

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
