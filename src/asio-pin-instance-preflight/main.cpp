#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <ks.h>
#include <ksmedia.h>

#include <cstdio>
#include <cwchar>

#pragma comment(lib, "setupapi.lib")

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error This probe must be built for native Windows ARM64, not ARM64EC.
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

    if (set == INVALID_HANDLE_VALUE) {
        std::printf("SetupDiGetClassDevsW failed Win32=%lu\n", GetLastError());
        return false;
    }

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

struct InstanceQueryResult {
    bool ok = false;
    DWORD win32_error = ERROR_SUCCESS;
    ULONG possible = 0;
    ULONG current = 0;
};

InstanceQueryResult query_instances(HANDLE filter, ULONG property_id) {
    KSP_PIN request{};
    request.Property.Set = KSPROPSETID_Pin;
    request.Property.Id = property_id;
    request.Property.Flags = KSPROPERTY_TYPE_GET;
    request.PinId = kRenderPinId;
    request.Reserved = 0;

    KSPIN_CINSTANCES instances{};
    DWORD returned = 0;

    InstanceQueryResult result{};
    if (!DeviceIoControl(
            filter,
            IOCTL_KS_PROPERTY,
            &request,
            sizeof(request),
            &instances,
            sizeof(instances),
            &returned,
            nullptr)) {
        result.win32_error = GetLastError();
        return result;
    }

    result.ok = true;
    result.possible = instances.PossibleCount;
    result.current = instances.CurrentCount;
    return result;
}

void print_result(const char* name, ULONG property_id, const InstanceQueryResult& r) {
    if (!r.ok) {
        std::printf("%s id=%lu -> FAILED Win32=%lu\n", name, property_id, r.win32_error);
        return;
    }

    const bool saturated = r.current >= r.possible;
    std::printf(
        "%s id=%lu PossibleCount=%lu CurrentCount=%lu saturated=%s\n",
        name,
        property_id,
        r.possible,
        r.current,
        saturated ? "YES" : "NO");
}

} // namespace

int main() {
    std::printf("Sound Blaster X4 Windows ARM64 - KS pin-instance preflight\n");
    std::printf("SAFETY: read-only property probe; no KsCreatePin; no KS state changes; no WaveRT buffer.\n");

    USHORT process_machine = IMAGE_FILE_MACHINE_UNKNOWN;
    USHORT native_machine = IMAGE_FILE_MACHINE_UNKNOWN;
    if (IsWow64Process2(GetCurrentProcess(), &process_machine, &native_machine)) {
        std::printf(
            "IsWow64Process2 processMachine=0x%04X nativeMachine=0x%04X\n",
            static_cast<unsigned>(process_machine),
            static_cast<unsigned>(native_machine));
    }

    wchar_t path[1024]{};
    if (!find_x4_wave_path(path, sizeof(path) / sizeof(path[0]))) {
        std::printf("RESULT: X4 msft_wave filter not found\n");
        return 2;
    }

    std::wprintf(L"X4 msft_wave=%ls\n", path);
    std::printf("RenderPinId=%lu\n", kRenderPinId);

    HANDLE filter = CreateFileW(
        path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (filter == INVALID_HANDLE_VALUE) {
        std::printf("CreateFileW(msft_wave) failed Win32=%lu\n", GetLastError());
        return 3;
    }

    const auto local = query_instances(filter, static_cast<ULONG>(KSPROPERTY_PIN_CINSTANCES));
    const auto global = query_instances(filter, static_cast<ULONG>(KSPROPERTY_PIN_GLOBALCINSTANCES));

    CloseHandle(filter);

    print_result("KSPROPERTY_PIN_CINSTANCES", static_cast<ULONG>(KSPROPERTY_PIN_CINSTANCES), local);
    print_result("KSPROPERTY_PIN_GLOBALCINSTANCES", static_cast<ULONG>(KSPROPERTY_PIN_GLOBALCINSTANCES), global);

    const bool local_saturated = local.ok && local.current >= local.possible;
    const bool global_saturated = global.ok && global.current >= global.possible;
    const bool creative_gate_busy = local_saturated || global_saturated;

    std::printf("Creative-equivalent gate busy=%s\n", creative_gate_busy ? "YES" : "NO");

    if (!local.ok && !global.ok) {
        std::printf("PREFLIGHT RESULT: INDETERMINATE\n");
        return 4;
    }

    std::printf("PREFLIGHT RESULT: %s\n", creative_gate_busy ? "BUSY" : "FREE");
    return creative_gate_busy ? 10 : 0;
}
