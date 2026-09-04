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
#include <limits>

#include "wavert_engine_b5.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error B5 WaveRT engine source must be built for native Windows ARM64, not ARM64EC.
#endif

namespace {

constexpr ULONG kRenderPinId = 1;
constexpr ULONG kCapturePinId = 4;

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
        &KSCATEGORY_AUDIO, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    for (DWORD index = 0; ; ++index) {
        SP_DEVICE_INTERFACE_DATA interface_data{};
        interface_data.cbSize = sizeof(interface_data);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &KSCATEGORY_AUDIO, index, &interface_data)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &interface_data, nullptr, 0, &required, nullptr);
        if (required == 0 || required > 8192) continue;

        alignas(SP_DEVICE_INTERFACE_DETAIL_DATA_W) BYTE storage[8192]{};
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage);
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(
                set, &interface_data, detail, static_cast<DWORD>(sizeof(storage)),
                &required, nullptr)) {
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
    ULONG pin_id,
    ULONG property_id,
    ULONG* possible,
    ULONG* current,
    DWORD* error_out) {

    KSP_PIN request{};
    request.Property.Set = KSPROPSETID_Pin;
    request.Property.Id = property_id;
    request.Property.Flags = KSPROPERTY_TYPE_GET;
    request.PinId = pin_id;

    KSPIN_CINSTANCES instances{};
    DWORD returned = 0;
    if (!DeviceIoControl(
            filter, IOCTL_KS_PROPERTY,
            &request, sizeof(request),
            &instances, sizeof(instances),
            &returned, nullptr)) {
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

PinCreateRequest make_pin_request(const X4WaveRtB5Config& config) {
    PinCreateRequest request{};
    request.Connect.Interface.Set = KSINTERFACESETID_Standard;
    request.Connect.Interface.Id = KSINTERFACE_STANDARD_LOOPED_STREAMING;
    request.Connect.Medium.Set = KSMEDIUMSETID_Standard;
    request.Connect.Medium.Id = KSMEDIUM_STANDARD_DEVIO;
    request.Connect.PinId = config.pin_id;
    request.Connect.PinToHandle = nullptr;
    request.Connect.Priority.PriorityClass = KSPRIORITY_NORMAL;
    request.Connect.Priority.PrioritySubClass = 1;

    request.Format.DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    request.Format.DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    request.Format.DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    request.Format.DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;

    const ULONG bytes_per_frame =
        static_cast<ULONG>(config.channels) * static_cast<ULONG>(config.bits_per_sample / 8u);
    auto& wave = request.Format.WaveFormatExt;
    wave.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wave.Format.nChannels = config.channels;
    wave.Format.nSamplesPerSec = config.sample_rate;
    wave.Format.nAvgBytesPerSec = config.sample_rate * bytes_per_frame;
    wave.Format.nBlockAlign = static_cast<WORD>(bytes_per_frame);
    wave.Format.wBitsPerSample = config.bits_per_sample;
    wave.Format.cbSize = static_cast<WORD>(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX));
    wave.Samples.wValidBitsPerSample = config.bits_per_sample;
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

bool set_state(HANDLE pin, KSSTATE state, const char* prefix) {
    KSPROPERTY property = make_property(
        KSPROPSETID_Connection,
        KSPROPERTY_CONNECTION_STATE,
        KSPROPERTY_TYPE_SET);
    DWORD returned = 0;
    const BOOL ok = DeviceIoControl(
        pin, IOCTL_KS_PROPERTY,
        &property, sizeof(property),
        &state, sizeof(state),
        &returned, nullptr);
    std::printf("B5 %s KSSTATE %u -> %s\n",
                prefix, static_cast<unsigned>(state), ok ? "OK" : "FAIL");
    return !!ok;
}

bool get_packet_count(HANDLE pin, ULONG* packet_count) {
    KSPROPERTY property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_PACKETCOUNT,
        KSPROPERTY_TYPE_GET);
    DWORD returned = 0;
    return !!DeviceIoControl(
        pin, IOCTL_KS_PROPERTY,
        &property, sizeof(property),
        packet_count, sizeof(*packet_count),
        &returned, nullptr);
}

bool get_presentation_position(HANDLE pin, KSAUDIO_PRESENTATION_POSITION* position) {
    KSPROPERTY property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_PRESENTATION_POSITION,
        KSPROPERTY_TYPE_GET);
    DWORD returned = 0;
    return !!DeviceIoControl(
        pin, IOCTL_KS_PROPERTY,
        &property, sizeof(property),
        position, sizeof(*position),
        &returned, nullptr);
}

bool get_read_packet(HANDLE pin, KSRTAUDIO_GETREADPACKET_INFO* info) {
    KSPROPERTY property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_GETREADPACKET,
        KSPROPERTY_TYPE_GET);
    DWORD returned = 0;
    return !!DeviceIoControl(
        pin, IOCTL_KS_PROPERTY,
        &property, sizeof(property),
        info, sizeof(*info),
        &returned, nullptr);
}

bool valid_config(const X4WaveRtB5Config& config) {
    if (config.channels != 2 || config.bits_per_sample != 24 || config.notification_count != 2) {
        return false;
    }
    if (config.frames_per_packet == 0) return false;
    if (config.sample_rate != 48000 && config.sample_rate != 96000 && config.sample_rate != 192000) {
        return false;
    }
    if (config.direction == X4WaveRtB5Direction::Render && config.pin_id != kRenderPinId) return false;
    if (config.direction == X4WaveRtB5Direction::Capture && config.pin_id != kCapturePinId) return false;
    if (config.direction == X4WaveRtB5Direction::Capture && config.sample_rate == 192000) return false;
    return true;
}

} // namespace

X4WaveRtEngineB5::~X4WaveRtEngineB5() {
    dispose();
}

X4WaveRtB5PrepareResult X4WaveRtEngineB5::prepare(const X4WaveRtB5Config& config) {
    dispose();
    stats_ = {};
    previous_packet_ = 0;
    have_previous_packet_ = false;
    previous_position_ = 0;
    have_previous_position_ = false;

    if (!valid_config(config)) {
        strcpy_s(last_message_, "B5 prepare FAILED: unsupported stream config");
        return X4WaveRtB5PrepareResult::Failed;
    }
    config_ = config;
    bytes_per_frame_ = static_cast<ULONG>(config.channels) * 3u;

    const unsigned long long packet_bytes64 =
        static_cast<unsigned long long>(config.frames_per_packet) * bytes_per_frame_;
    const unsigned long long requested_bytes64 =
        packet_bytes64 * static_cast<unsigned long long>(config.notification_count);
    if (packet_bytes64 > std::numeric_limits<ULONG>::max() ||
        requested_bytes64 > std::numeric_limits<ULONG>::max()) {
        strcpy_s(last_message_, "B5 prepare FAILED: buffer geometry overflow");
        return X4WaveRtB5PrepareResult::Failed;
    }
    packet_bytes_ = static_cast<ULONG>(packet_bytes64);
    const ULONG requested_buffer_bytes = static_cast<ULONG>(requested_bytes64);

    wchar_t path[1024]{};
    if (!find_x4_wave_path(path, _countof(path))) {
        strcpy_s(last_message_, "B5 prepare FAILED: X4 msft_wave not found");
        return X4WaveRtB5PrepareResult::Failed;
    }

    filter_ = CreateFileW(
        path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (filter_ == INVALID_HANDLE_VALUE) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "B5 prepare FAILED: filter open Win32=%lu", GetLastError());
        return X4WaveRtB5PrepareResult::Failed;
    }

    ULONG local_possible = 0;
    ULONG local_current = 0;
    ULONG global_possible = 0;
    ULONG global_current = 0;
    DWORD local_error = ERROR_SUCCESS;
    DWORD global_error = ERROR_SUCCESS;

    const bool local_ok = query_instances(
        filter_, config.pin_id, KSPROPERTY_PIN_CINSTANCES,
        &local_possible, &local_current, &local_error);
    const bool global_ok = query_instances(
        filter_, config.pin_id, KSPROPERTY_PIN_GLOBALCINSTANCES,
        &global_possible, &global_current, &global_error);

    const char* dir = config.direction == X4WaveRtB5Direction::Render ? "RENDER" : "CAPTURE";
    if (!local_ok || !global_ok || local_possible == 0 || global_possible == 0) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "B5 %s PRE-PIN INDETERMINATE pin=%lu C ok=%d %lu/%lu err=%lu G ok=%d %lu/%lu err=%lu; KsCreatePin SKIPPED",
                  dir, config.pin_id,
                  local_ok ? 1 : 0, local_current, local_possible, local_error,
                  global_ok ? 1 : 0, global_current, global_possible, global_error);
        CloseHandle(filter_);
        filter_ = INVALID_HANDLE_VALUE;
        return X4WaveRtB5PrepareResult::Indeterminate;
    }

    const bool busy =
        local_current >= local_possible || global_current >= global_possible;
    std::printf("B5 %s PRE-PIN GATE pin=%lu C %lu/%lu G %lu/%lu busy=%s\n",
                dir, config.pin_id,
                local_current, local_possible, global_current, global_possible,
                busy ? "YES" : "NO");
    if (busy) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "B5 %s PRE-PIN BUSY pin=%lu C %lu/%lu G %lu/%lu; KsCreatePin SKIPPED",
                  dir, config.pin_id,
                  local_current, local_possible, global_current, global_possible);
        CloseHandle(filter_);
        filter_ = INVALID_HANDLE_VALUE;
        return X4WaveRtB5PrepareResult::Busy;
    }

    PinCreateRequest pin_request = make_pin_request(config);
    HANDLE created_pin = nullptr;
    const ACCESS_MASK access =
        config.direction == X4WaveRtB5Direction::Render ? GENERIC_WRITE : GENERIC_READ;
    const DWORD status = KsCreatePin(filter_, &pin_request.Connect, access, &created_pin);
    if (status != ERROR_SUCCESS || !created_pin || created_pin == INVALID_HANDLE_VALUE) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "B5 %s KsCreatePin FAILED pin=%lu status=0x%08lX rate=%lu bits=%u frames=%lu",
                  dir, config.pin_id, static_cast<unsigned long>(status),
                  config.sample_rate, config.bits_per_sample, config.frames_per_packet);
        CloseHandle(filter_);
        filter_ = INVALID_HANDLE_VALUE;
        return X4WaveRtB5PrepareResult::Failed;
    }
    pin_ = created_pin;

    KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION buffer_request{};
    buffer_request.Property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION,
        KSPROPERTY_TYPE_GET);
    buffer_request.BaseAddress = nullptr;
    buffer_request.RequestedBufferSize = requested_buffer_bytes;
    buffer_request.NotificationCount = config.notification_count;

    KSRTAUDIO_BUFFER buffer{};
    DWORD returned = 0;
    if (!DeviceIoControl(
            pin_, IOCTL_KS_PROPERTY,
            &buffer_request, sizeof(buffer_request),
            &buffer, sizeof(buffer),
            &returned, nullptr)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "B5 %s BUFFER_WITH_NOTIFICATION FAILED Win32=%lu requested=%lu",
                  dir, GetLastError(), requested_buffer_bytes);
        dispose();
        return X4WaveRtB5PrepareResult::Failed;
    }

    if (!buffer.BufferAddress || buffer.ActualBufferSize != requested_buffer_bytes) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "B5 %s WaveRT geometry mismatch requested=%lu actual=%lu address=%p",
                  dir, requested_buffer_bytes, buffer.ActualBufferSize, buffer.BufferAddress);
        dispose();
        return X4WaveRtB5PrepareResult::Failed;
    }

    buffer_address_ = buffer.BufferAddress;
    actual_buffer_size_ = buffer.ActualBufferSize;
    call_memory_barrier_ = !!buffer.CallMemoryBarrier;
    ZeroMemory(buffer_address_, actual_buffer_size_);
    if (call_memory_barrier_) MemoryBarrier();

    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "B5 %s CreateEventW FAILED Win32=%lu", dir, GetLastError());
        dispose();
        return X4WaveRtB5PrepareResult::Failed;
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
                  "B5 %s REGISTER_NOTIFICATION_EVENT FAILED Win32=%lu",
                  dir, GetLastError());
        dispose();
        return X4WaveRtB5PrepareResult::Failed;
    }

    notification_registered_ = true;
    prepared_ = true;
    sprintf_s(last_message_, sizeof(last_message_),
              "B5 %s prepared pin=%lu rate=%lu bits=24 frames=%lu packetBytes=%lu cyclicBytes=%lu",
              dir, config.pin_id, config.sample_rate, config.frames_per_packet,
              packet_bytes_, actual_buffer_size_);
    std::printf("%s\n", last_message_);
    return X4WaveRtB5PrepareResult::Ready;
}

bool X4WaveRtEngineB5::start_run() {
    if (!prepared_ || pin_ == INVALID_HANDLE_VALUE || !event_) {
        strcpy_s(last_message_, "B5 start FAILED: engine not prepared");
        return false;
    }
    if (entered_run_) {
        strcpy_s(last_message_, "B5 start FAILED: already RUN");
        return false;
    }

    stats_ = {};
    previous_packet_ = 0;
    have_previous_packet_ = false;
    previous_position_ = 0;
    have_previous_position_ = false;

    const char* dir = config_.direction == X4WaveRtB5Direction::Render ? "RENDER" : "CAPTURE";
    if (!set_state(pin_, KSSTATE_ACQUIRE, dir) ||
        !set_state(pin_, KSSTATE_PAUSE, dir) ||
        !set_state(pin_, KSSTATE_RUN, dir)) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "B5 %s state transition FAILED Win32=%lu", dir, GetLastError());
        return false;
    }

    entered_run_ = true;
    sprintf_s(last_message_, sizeof(last_message_), "B5 %s RUN entered", dir);
    return true;
}

X4WaveRtB5ProcessResult X4WaveRtEngineB5::process_one_notification(
    HANDLE stop_event,
    DWORD timeout_ms,
    X4WaveRtB5NotificationObserver observer,
    void* observer_context) {

    if (!entered_run_ || !event_ || pin_ == INVALID_HANDLE_VALUE || !stop_event) {
        strcpy_s(last_message_, "B5 process FAILED: RUN/event/stop state invalid");
        return X4WaveRtB5ProcessResult::Failed;
    }

    HANDLE handles[2] = {stop_event, event_};
    const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, timeout_ms);
    if (wait == WAIT_OBJECT_0) return X4WaveRtB5ProcessResult::StopRequested;
    if (wait != WAIT_OBJECT_0 + 1) {
        if (wait == WAIT_TIMEOUT) {
            sprintf_s(last_message_, sizeof(last_message_),
                      "B5 worker FAILED: notification timeout=%lu ms", timeout_ms);
        } else {
            sprintf_s(last_message_, sizeof(last_message_),
                      "B5 worker FAILED: wait=%lu Win32=%lu", wait, GetLastError());
        }
        return X4WaveRtB5ProcessResult::Failed;
    }

    ULONG packet_number = 0;
    UINT64 position_blocks = 0;
    UINT64 qpc = 0;
    BOOL more_data = FALSE;

    if (config_.direction == X4WaveRtB5Direction::Render) {
        if (!get_packet_count(pin_, &packet_number)) {
            sprintf_s(last_message_, sizeof(last_message_),
                      "B5 RENDER PACKETCOUNT FAILED Win32=%lu", GetLastError());
            return X4WaveRtB5ProcessResult::Failed;
        }
        KSAUDIO_PRESENTATION_POSITION position{};
        if (!get_presentation_position(pin_, &position)) {
            sprintf_s(last_message_, sizeof(last_message_),
                      "B5 RENDER PRESENTATION_POSITION FAILED Win32=%lu", GetLastError());
            return X4WaveRtB5ProcessResult::Failed;
        }
        position_blocks = position.u64PositionInBlocks;
        qpc = position.u64QPCPosition;
        if (have_previous_position_ && position_blocks < previous_position_) {
            ++stats_.position_regressions;
        }
        previous_position_ = position_blocks;
        have_previous_position_ = true;
    } else {
        KSRTAUDIO_GETREADPACKET_INFO info{};
        if (!get_read_packet(pin_, &info)) {
            sprintf_s(last_message_, sizeof(last_message_),
                      "B5 CAPTURE GETREADPACKET FAILED Win32=%lu", GetLastError());
            return X4WaveRtB5ProcessResult::Failed;
        }
        packet_number = info.PacketNumber;
        qpc = info.PerformanceCount;
        more_data = info.MoreData;
    }

    if (have_previous_packet_ && packet_number != previous_packet_ + 1) {
        ++stats_.packet_discontinuities;
    }
    previous_packet_ = packet_number;
    have_previous_packet_ = true;

    const ULONG notification_index = stats_.notifications;
    ++stats_.notifications;
    stats_.last_packet = packet_number;

    const char* dir = config_.direction == X4WaveRtB5Direction::Render ? "RENDER" : "CAPTURE";
    std::printf(
        "B5 %s notification=%lu packet=%lu slot=%lu position=%llu qpc=%llu moreData=%d thread=%lu\n",
        dir, stats_.notifications, packet_number,
        packet_number % config_.notification_count,
        static_cast<unsigned long long>(position_blocks),
        static_cast<unsigned long long>(qpc), more_data ? 1 : 0,
        GetCurrentThreadId());

    if (observer && !observer(observer_context, notification_index, packet_number)) {
        return X4WaveRtB5ProcessResult::Failed;
    }
    return X4WaveRtB5ProcessResult::Notification;
}

bool X4WaveRtEngineB5::write_render_packet24(
    ULONG absolute_packet_number,
    const std::uint8_t* left,
    const std::uint8_t* right,
    ULONG frames) {

    if (config_.direction != X4WaveRtB5Direction::Render || !prepared_ || !buffer_address_) {
        strcpy_s(last_message_, "B5 render copy FAILED: render buffer not prepared");
        return false;
    }
    if (!left || !right || frames != config_.frames_per_packet || bytes_per_frame_ != 6) {
        strcpy_s(last_message_, "B5 render copy FAILED: expected planar Int24LSB stereo matching packet frames");
        return false;
    }

    const ULONG slot = absolute_packet_number % config_.notification_count;
    const ULONG byte_offset = slot * packet_bytes_;
    if (byte_offset + packet_bytes_ > actual_buffer_size_) {
        strcpy_s(last_message_, "B5 render copy FAILED: packet offset out of range");
        return false;
    }

    auto* target = static_cast<std::uint8_t*>(buffer_address_) + byte_offset;
    ULONG nonzero = 0;
    for (ULONG frame = 0; frame < frames; ++frame) {
        const std::uint8_t* l = left + frame * 3u;
        const std::uint8_t* r = right + frame * 3u;
        std::uint8_t* out = target + frame * 6u;
        out[0] = l[0]; out[1] = l[1]; out[2] = l[2];
        out[3] = r[0]; out[4] = r[1]; out[5] = r[2];
        if (l[0] || l[1] || l[2]) ++nonzero;
        if (r[0] || r[1] || r[2]) ++nonzero;
    }
    if (call_memory_barrier_) MemoryBarrier();

    ++stats_.hardware_buffer_writes;
    stats_.dma_frames_copied += frames;
    stats_.dma_nonzero_samples += nonzero;
    stats_.last_packet = absolute_packet_number;
    return true;
}

bool X4WaveRtEngineB5::read_capture_packet24(
    ULONG absolute_packet_number,
    std::uint8_t* left,
    std::uint8_t* right,
    ULONG frames) {

    if (config_.direction != X4WaveRtB5Direction::Capture || !prepared_ || !buffer_address_) {
        strcpy_s(last_message_, "B5 capture copy FAILED: capture buffer not prepared");
        return false;
    }
    if (!left || !right || frames != config_.frames_per_packet || bytes_per_frame_ != 6) {
        strcpy_s(last_message_, "B5 capture copy FAILED: expected planar Int24LSB stereo matching packet frames");
        return false;
    }

    const ULONG slot = absolute_packet_number % config_.notification_count;
    const ULONG byte_offset = slot * packet_bytes_;
    if (byte_offset + packet_bytes_ > actual_buffer_size_) {
        strcpy_s(last_message_, "B5 capture copy FAILED: packet offset out of range");
        return false;
    }

    if (call_memory_barrier_) MemoryBarrier();
    const auto* source = static_cast<const std::uint8_t*>(buffer_address_) + byte_offset;
    ULONG nonzero = 0;
    for (ULONG frame = 0; frame < frames; ++frame) {
        const std::uint8_t* in = source + frame * 6u;
        std::uint8_t* l = left + frame * 3u;
        std::uint8_t* r = right + frame * 3u;
        l[0] = in[0]; l[1] = in[1]; l[2] = in[2];
        r[0] = in[3]; r[1] = in[4]; r[2] = in[5];
        if (l[0] || l[1] || l[2]) ++nonzero;
        if (r[0] || r[1] || r[2]) ++nonzero;
    }

    ++stats_.hardware_buffer_writes;
    stats_.dma_frames_copied += frames;
    stats_.dma_nonzero_samples += nonzero;
    stats_.last_packet = absolute_packet_number;
    return true;
}

bool X4WaveRtEngineB5::stop() {
    if (!entered_run_) return true;
    bool ok = true;
    const char* dir = config_.direction == X4WaveRtB5Direction::Render ? "RENDER" : "CAPTURE";
    if (!set_state(pin_, KSSTATE_PAUSE, dir)) ok = false;
    if (!set_state(pin_, KSSTATE_ACQUIRE, dir)) ok = false;
    if (!set_state(pin_, KSSTATE_STOP, dir)) ok = false;
    entered_run_ = false;
    if (!ok) {
        sprintf_s(last_message_, sizeof(last_message_),
                  "B5 %s stop FAILED Win32=%lu", dir, GetLastError());
    }
    return ok;
}

void X4WaveRtEngineB5::dispose() {
    if (entered_run_) stop();

    if (notification_registered_ && pin_ != INVALID_HANDLE_VALUE && event_) {
        KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY notification{};
        notification.Property = make_property(
            KSPROPSETID_RtAudio,
            KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT,
            KSPROPERTY_TYPE_SET);
        notification.NotificationEvent = event_;
        DWORD returned = 0;
        DeviceIoControl(
            pin_, IOCTL_KS_PROPERTY,
            &notification, sizeof(notification),
            nullptr, 0, &returned, nullptr);
    }

    notification_registered_ = false;
    buffer_address_ = nullptr;
    actual_buffer_size_ = 0;
    packet_bytes_ = 0;
    bytes_per_frame_ = 0;
    call_memory_barrier_ = false;
    previous_packet_ = 0;
    have_previous_packet_ = false;
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
    config_ = {};
}
