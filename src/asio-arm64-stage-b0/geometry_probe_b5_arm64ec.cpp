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
#include <cwchar>

#if !defined(_M_ARM64EC)
#error B5 192 kHz geometry probe must be compiled as ARM64EC.
#endif

namespace {

constexpr ULONG kRenderPinId = 1;
constexpr ULONG kSampleRate = 192000;
constexpr USHORT kChannels = 2;
constexpr USHORT kBitsPerSample = 24;
constexpr ULONG kBytesPerFrame = 6;
constexpr ULONG kNotificationCount = 2;

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

struct GateSnapshot {
    bool local_ok = false;
    bool global_ok = false;
    ULONG local_possible = 0;
    ULONG local_current = 0;
    ULONG global_possible = 0;
    ULONG global_current = 0;
    DWORD local_error = ERROR_SUCCESS;
    DWORD global_error = ERROR_SUCCESS;
};

GateSnapshot read_gate(HANDLE filter) {
    GateSnapshot gate{};
    gate.local_ok = query_instances(
        filter, KSPROPERTY_PIN_CINSTANCES,
        &gate.local_possible, &gate.local_current, &gate.local_error);
    gate.global_ok = query_instances(
        filter, KSPROPERTY_PIN_GLOBALCINSTANCES,
        &gate.global_possible, &gate.global_current, &gate.global_error);
    return gate;
}

bool gate_free(const GateSnapshot& gate) {
    return gate.local_ok && gate.global_ok &&
           gate.local_possible > 0 && gate.global_possible > 0 &&
           gate.local_current < gate.local_possible &&
           gate.global_current < gate.global_possible;
}

void print_gate(const char* prefix, const GateSnapshot& gate) {
    std::printf(
        "%s C ok=%d %lu/%lu err=%lu G ok=%d %lu/%lu err=%lu free=%s\n",
        prefix,
        gate.local_ok ? 1 : 0, gate.local_current, gate.local_possible, gate.local_error,
        gate.global_ok ? 1 : 0, gate.global_current, gate.global_possible, gate.global_error,
        gate_free(gate) ? "YES" : "NO");
}

bool wait_for_own_pin_release(HANDLE filter) {
    for (int i = 0; i < 40; ++i) {
        const GateSnapshot gate = read_gate(filter);
        if (gate_free(gate)) return true;
        Sleep(25);
    }
    print_gate("B5 192K GEOMETRY post-close gate", read_gate(filter));
    return false;
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

int probe_candidate(HANDLE filter, ULONG packet_frames, bool* success_out) {
    if (success_out) *success_out = false;

    const GateSnapshot gate = read_gate(filter);
    if (!gate_free(gate)) {
        print_gate("B5 192K GEOMETRY pre-pin gate BLOCKED", gate);
        return 10;
    }

    PinCreateRequest pin_request = make_pin_request();
    HANDLE pin = nullptr;
    const DWORD create_status = KsCreatePin(
        filter, &pin_request.Connect, GENERIC_WRITE, &pin);
    if (create_status != ERROR_SUCCESS || !pin || pin == INVALID_HANDLE_VALUE) {
        std::printf(
            "B5 192K GEOMETRY pin-create FAIL status=0x%08lX frames=%lu\n",
            static_cast<unsigned long>(create_status), packet_frames);
        return 20;
    }

    const ULONG packet_bytes = packet_frames * kBytesPerFrame;
    const ULONG requested_bytes = packet_bytes * kNotificationCount;
    const ULONG period_us = static_cast<ULONG>(
        (static_cast<unsigned long long>(packet_frames) * 1000000ull) / kSampleRate);

    KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION request{};
    request.Property = make_property(
        KSPROPSETID_RtAudio,
        KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION,
        KSPROPERTY_TYPE_GET);
    request.BaseAddress = nullptr;
    request.RequestedBufferSize = requested_bytes;
    request.NotificationCount = kNotificationCount;

    KSRTAUDIO_BUFFER buffer{};
    DWORD returned = 0;
    SetLastError(ERROR_SUCCESS);
    const BOOL ok = DeviceIoControl(
        pin, IOCTL_KS_PROPERTY,
        &request, sizeof(request),
        &buffer, sizeof(buffer),
        &returned, nullptr);
    const DWORD error = ok ? ERROR_SUCCESS : GetLastError();

    if (ok) {
        const ULONG actual_frames_per_notification =
            (buffer.ActualBufferSize / kNotificationCount) / kBytesPerFrame;
        std::printf(
            "B5 192K GEOMETRY candidate framesPerNotif=%lu periodUs=%lu packetBytes=%lu requestedBytes=%lu result=PASS actualBytes=%lu actualFramesPerNotif=%lu callBarrier=%d\n",
            packet_frames, period_us, packet_bytes, requested_bytes,
            buffer.ActualBufferSize, actual_frames_per_notification,
            buffer.CallMemoryBarrier ? 1 : 0);
        if (success_out) *success_out = true;
    } else {
        std::printf(
            "B5 192K GEOMETRY candidate framesPerNotif=%lu periodUs=%lu packetBytes=%lu requestedBytes=%lu result=FAIL win32=%lu\n",
            packet_frames, period_us, packet_bytes, requested_bytes, error);
    }

    CloseHandle(pin);
    pin = nullptr;

    if (!wait_for_own_pin_release(filter)) {
        std::puts("B5 192K GEOMETRY STOP: render pin did not return to FREE after close; no further KsCreatePin attempts");
        return 11;
    }

    return 0;
}

} // namespace

int wmain() {
    std::puts("Sound Blaster X4 B5 192 kHz WaveRT geometry probe");
    std::puts("SAFETY: property/buffer allocation only; KSSTATE_RUN is never entered; Render Pin 1 FREE gate is checked before every KsCreatePin");
    std::printf("format rate=%lu channels=%u bits=%u bytesPerFrame=%lu notificationCount=%lu\n",
                kSampleRate, kChannels, kBitsPerSample, kBytesPerFrame, kNotificationCount);

    wchar_t path[1024]{};
    if (!find_x4_wave_path(path, _countof(path))) {
        std::puts("B5 192K GEOMETRY RESULT: FAIL X4 msft_wave not found");
        return 2;
    }
    std::wprintf(L"devicePath=%ls\n", path);

    HANDLE filter = CreateFileW(
        path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (filter == INVALID_HANDLE_VALUE) {
        std::printf("B5 192K GEOMETRY RESULT: FAIL filter open Win32=%lu\n", GetLastError());
        return 3;
    }

    const GateSnapshot initial_gate = read_gate(filter);
    print_gate("B5 192K GEOMETRY initial gate", initial_gate);
    if (!gate_free(initial_gate)) {
        std::puts("B5 192K GEOMETRY RESULT: BUSY_OR_INDETERMINATE; KsCreatePin NEVER CALLED");
        CloseHandle(filter);
        return 10;
    }

    ULONG successes = 0;
    ULONG first_success_frames = 0;
    ULONG first_success_bytes = 0;

    // 48 frames at 192 kHz = 0.25 ms. Scan 0.25 ms increments through 5 ms.
    // This brackets the failing ASIO-240 geometry (1.25 ms per notification)
    // and tests common USB/WaveRT service-period boundaries without touching RUN.
    for (ULONG packet_frames = 48; packet_frames <= 960; packet_frames += 48) {
        bool success = false;
        const int result = probe_candidate(filter, packet_frames, &success);
        if (result != 0) {
            CloseHandle(filter);
            std::printf("B5 192K GEOMETRY RESULT: ABORT code=%d\n", result);
            return result;
        }
        if (success) {
            ++successes;
            if (first_success_frames == 0) {
                first_success_frames = packet_frames;
                first_success_bytes = packet_frames * kBytesPerFrame * kNotificationCount;
            }
        }
    }

    CloseHandle(filter);

    if (successes == 0) {
        std::puts("B5 192K GEOMETRY RESULT: FAIL no accepted geometry in 48..960 frames/notification");
        return 30;
    }

    std::printf(
        "B5 192K GEOMETRY RESULT: PASS accepted=%lu firstAcceptedFramesPerNotif=%lu firstAcceptedRequestedBytes=%lu\n",
        successes, first_success_frames, first_success_bytes);
    return 0;
}
