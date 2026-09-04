#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <winioctl.h>
#include <setupapi.h>
#include <ks.h>
#include <ksmedia.h>

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#if !defined(_M_ARM64EC)
#error B5 KS capability probe must be compiled as ARM64EC.
#endif

namespace {

constexpr ULONG kRenderPin = 1;

bool contains_i(const wchar_t* text, const wchar_t* needle) {
    if (!text || !needle || !*needle) return false;
    for (const wchar_t* p = text; *p; ++p) {
        const wchar_t* a = p;
        const wchar_t* b = needle;
        while (*a && *b) {
            wchar_t ca = *a, cb = *b;
            if (ca >= L'A' && ca <= L'Z') ca += L'a' - L'A';
            if (cb >= L'A' && cb <= L'Z') cb += L'a' - L'A';
            if (ca != cb) break;
            ++a; ++b;
        }
        if (!*b) return true;
    }
    return false;
}

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), n, nullptr, nullptr);
    out.pop_back();
    return out;
}

bool x4_path(std::wstring* out) {
    if (!out) return false;
    HDEVINFO set = SetupDiGetClassDevsW(
        &KSCATEGORY_AUDIO, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA item{};
        item.cbSize = sizeof(item);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &KSCATEGORY_AUDIO, index, &item)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &item, nullptr, 0, &required, nullptr);
        if (!required || required > 16384) continue;

        alignas(SP_DEVICE_INTERFACE_DETAIL_DATA_W) BYTE storage[16384]{};
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage);
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(
                set, &item, detail, static_cast<DWORD>(sizeof(storage)), &required, nullptr)) continue;

        if (contains_i(detail->DevicePath, L"vid_041e&pid_3278&mi_03") &&
            contains_i(detail->DevicePath, L"\\msft_wave")) {
            *out = detail->DevicePath;
            found = true;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(set);
    return found;
}

KSPROPERTY property(const GUID& set, ULONG id) {
    KSPROPERTY p{};
    p.Set = set;
    p.Id = id;
    p.Flags = KSPROPERTY_TYPE_GET;
    return p;
}

template <typename T>
bool pin_scalar(HANDLE filter, ULONG pin, ULONG id, T* value, DWORD* error) {
    KSP_PIN request{};
    request.Property = property(KSPROPSETID_Pin, id);
    request.PinId = pin;
    DWORD returned = 0;
    if (!DeviceIoControl(filter, IOCTL_KS_PROPERTY, &request, sizeof(request),
                         value, sizeof(*value), &returned, nullptr)) {
        if (error) *error = GetLastError();
        return false;
    }
    if (returned < sizeof(*value)) {
        if (error) *error = ERROR_INSUFFICIENT_BUFFER;
        return false;
    }
    if (error) *error = ERROR_SUCCESS;
    return true;
}

bool instances(HANDLE filter, ULONG pin, ULONG id, KSPIN_CINSTANCES* out, DWORD* error) {
    return pin_scalar(filter, pin, id, out, error);
}

bool pin_blob(HANDLE filter, ULONG pin, ULONG id, std::vector<BYTE>* out, DWORD* error) {
    if (!out) return false;
    KSP_PIN request{};
    request.Property = property(KSPROPSETID_Pin, id);
    request.PinId = pin;

    DWORD needed = 0;
    BOOL first = DeviceIoControl(filter, IOCTL_KS_PROPERTY, &request, sizeof(request),
                                 nullptr, 0, &needed, nullptr);
    if (!first) {
        const DWORD e = GetLastError();
        if (e != ERROR_MORE_DATA && e != ERROR_INSUFFICIENT_BUFFER) {
            if (error) *error = e;
            return false;
        }
    }
    if (needed < sizeof(KSMULTIPLE_ITEM) || needed > 1024 * 1024) {
        if (error) *error = ERROR_INVALID_DATA;
        return false;
    }

    out->assign(needed, 0);
    DWORD returned = 0;
    if (!DeviceIoControl(filter, IOCTL_KS_PROPERTY, &request, sizeof(request),
                         out->data(), static_cast<DWORD>(out->size()), &returned, nullptr)) {
        if (error) *error = GetLastError();
        return false;
    }
    out->resize(returned);
    if (error) *error = ERROR_SUCCESS;
    return true;
}

bool pin_count(HANDLE filter, ULONG* count, DWORD* error) {
    KSPROPERTY request = property(KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES);
    DWORD returned = 0;
    if (!DeviceIoControl(filter, IOCTL_KS_PROPERTY, &request, sizeof(request),
                         count, sizeof(*count), &returned, nullptr)) {
        if (error) *error = GetLastError();
        return false;
    }
    if (error) *error = ERROR_SUCCESS;
    return returned >= sizeof(*count);
}

std::string guid(const GUID& value) {
    wchar_t text[64]{};
    StringFromGUID2(value, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
    return utf8(text);
}

const char* flow_name(KSPIN_DATAFLOW flow) {
    return flow == KSPIN_DATAFLOW_IN ? "IN(render-to-device)" :
           flow == KSPIN_DATAFLOW_OUT ? "OUT(capture-from-device)" : "UNKNOWN";
}

void dump_ranges(const std::vector<BYTE>& blob, ULONG pin) {
    if (blob.size() < sizeof(KSMULTIPLE_ITEM)) return;
    const auto* multi = reinterpret_cast<const KSMULTIPLE_ITEM*>(blob.data());
    std::printf("pin=%lu dataRangeCount=%lu totalSize=%lu\n",
                pin, multi->Count, multi->Size);

    size_t offset = sizeof(KSMULTIPLE_ITEM);
    for (ULONG i = 0; i < multi->Count; ++i) {
        if (offset + sizeof(KSDATARANGE) > blob.size()) {
            std::printf("pin=%lu range=%lu parse=TRUNCATED_HEADER\n", pin, i);
            break;
        }
        const auto* range = reinterpret_cast<const KSDATARANGE*>(blob.data() + offset);
        if (range->FormatSize < sizeof(KSDATARANGE) ||
            offset + range->FormatSize > blob.size()) {
            std::printf("pin=%lu range=%lu parse=INVALID_SIZE size=%lu\n",
                        pin, i, range->FormatSize);
            break;
        }

        std::printf("pin=%lu range=%lu size=%lu major=%s sub=%s specifier=%s",
                    pin, i, range->FormatSize, guid(range->MajorFormat).c_str(),
                    guid(range->SubFormat).c_str(), guid(range->Specifier).c_str());

        if (IsEqualGUID(range->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) &&
            range->FormatSize >= sizeof(KSDATARANGE_AUDIO)) {
            const auto* audio = reinterpret_cast<const KSDATARANGE_AUDIO*>(range);
            std::printf(" audio maxChannels=%lu bits=%lu..%lu rate=%lu..%lu",
                        audio->MaximumChannels,
                        audio->MinimumBitsPerSample, audio->MaximumBitsPerSample,
                        audio->MinimumSampleFrequency, audio->MaximumSampleFrequency);
        }
        std::puts("");
        offset += (static_cast<size_t>(range->FormatSize) + 7u) & ~static_cast<size_t>(7u);
    }
}

int idle_gate(HANDLE filter) {
    KSPIN_CINSTANCES local{}, global{};
    DWORD le = 0, ge = 0;
    const bool lok = instances(filter, kRenderPin, KSPROPERTY_PIN_CINSTANCES, &local, &le);
    const bool gok = instances(filter, kRenderPin, KSPROPERTY_PIN_GLOBALCINSTANCES, &global, &ge);

    if (!lok || !gok || !local.PossibleCount || !global.PossibleCount) {
        std::printf("B5 KS IDLE GATE: INDETERMINATE localOK=%d localErr=%lu globalOK=%d globalErr=%lu\n",
                    lok ? 1 : 0, le, gok ? 1 : 0, ge);
        return 11;
    }

    const bool busy = local.CurrentCount >= local.PossibleCount ||
                      global.CurrentCount >= global.PossibleCount;
    std::printf("B5 KS IDLE GATE: C %lu/%lu G %lu/%lu busy=%s; KsCreatePin NEVER CALLED\n",
                local.CurrentCount, local.PossibleCount,
                global.CurrentCount, global.PossibleCount,
                busy ? "YES" : "NO");
    return busy ? 10 : 0;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    bool require_idle = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--require-idle") == 0) require_idle = true;
        else {
            std::puts("Usage: x4-asio-stage-b5-ks-probe.exe [--require-idle]");
            return 2;
        }
    }

    std::wstring path;
    if (!x4_path(&path)) {
        std::puts("X4 msft_wave interface not found.");
        return 3;
    }
    std::printf("devicePath=%s\n", utf8(path).c_str());

    HANDLE filter = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (filter == INVALID_HANDLE_VALUE) {
        std::printf("filterOpen=FAIL Win32=%lu\n", GetLastError());
        return 4;
    }
    std::puts("filterOpen=OK property-only probe; KsCreatePin is not present in this program");

    const int gate = require_idle ? idle_gate(filter) : 0;

    ULONG count = 0;
    DWORD count_error = 0;
    if (pin_count(filter, &count, &count_error)) {
        std::printf("pinCount=%lu\n", count);
        for (ULONG pin = 0; pin < count; ++pin) {
            KSPIN_DATAFLOW flow{};
            DWORD flow_error = 0;
            const bool flow_ok = pin_scalar(
                filter, pin, KSPROPERTY_PIN_DATAFLOW, &flow, &flow_error);

            KSPIN_CINSTANCES local{}, global{};
            DWORD local_error = 0, global_error = 0;
            const bool local_ok = instances(
                filter, pin, KSPROPERTY_PIN_CINSTANCES, &local, &local_error);
            const bool global_ok = instances(
                filter, pin, KSPROPERTY_PIN_GLOBALCINSTANCES, &global, &global_error);

            std::printf("pin=%lu flow=%s flowOK=%d flowErr=%lu C=%s",
                        pin, flow_ok ? flow_name(flow) : "?", flow_ok ? 1 : 0, flow_error,
                        local_ok ? "OK" : "ERR");
            if (local_ok) std::printf("(%lu/%lu)", local.CurrentCount, local.PossibleCount);
            else std::printf("(%lu)", local_error);
            std::printf(" G=%s", global_ok ? "OK" : "ERR");
            if (global_ok) std::printf("(%lu/%lu)", global.CurrentCount, global.PossibleCount);
            else std::printf("(%lu)", global_error);
            std::puts("");

            std::vector<BYTE> ranges;
            DWORD range_error = 0;
            if (pin_blob(filter, pin, KSPROPERTY_PIN_DATARANGES, &ranges, &range_error)) {
                dump_ranges(ranges, pin);
            } else {
                std::printf("pin=%lu dataRanges=FAIL Win32=%lu\n", pin, range_error);
            }
        }
    } else {
        std::printf("pinCount=FAIL Win32=%lu\n", count_error);
    }

    CloseHandle(filter);
    std::printf("B5 KS CAPABILITY PROBE RESULT: %s\n",
                gate == 0 ? "PASS" : gate == 10 ? "BUSY_BLOCKED" : "INDETERMINATE_BLOCKED");
    return gate;
}
