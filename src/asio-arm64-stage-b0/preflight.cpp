#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <ks.h>
#include <ksmedia.h>

#include <cwchar>

#include "preflight.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error Stage B1 preflight must be built for native Windows ARM64, not ARM64EC.
#endif

namespace {

constexpr ULONG kRenderPinId = 1;

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

} // namespace

X4InstancePreflightResult run_x4_instance_preflight() {
    X4InstancePreflightResult result{};

    wchar_t path[1024]{};
    if (!find_x4_wave_path(path, sizeof(path) / sizeof(path[0]))) {
        return result;
    }
    result.device_found = true;

    HANDLE filter = CreateFileW(
        path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (filter == INVALID_HANDLE_VALUE) {
        result.open_error = GetLastError();
        return result;
    }
    result.filter_opened = true;

    result.local_ok = query_instances(
        filter,
        static_cast<ULONG>(KSPROPERTY_PIN_CINSTANCES),
        &result.local_possible,
        &result.local_current,
        &result.local_error);

    result.global_ok = query_instances(
        filter,
        static_cast<ULONG>(KSPROPERTY_PIN_GLOBALCINSTANCES),
        &result.global_possible,
        &result.global_current,
        &result.global_error);

    CloseHandle(filter);
    return result;
}
