#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winioctl.h>
#include <ks.h>
#include <cstdio>

// Experiment-only interception for the single KsCreatePin call in main.cpp.
// The real KsCreatePin declaration is visible before the macro below is defined.
static inline NTSTATUS WINAPI X4GuardedKsCreatePin(
    HANDLE filter_handle,
    PKSPIN_CONNECT connect,
    ACCESS_MASK desired_access,
    PHANDLE connection_handle) {

    if (!connect) {
        std::printf("GLOBAL INSTANCE GATE: Connect=NULL; KsCreatePin SKIPPED\n");
        std::fflush(stdout);
        return static_cast<NTSTATUS>(ERROR_INVALID_PARAMETER);
    }

    KSP_PIN request{};
    request.Property.Set = KSPROPSETID_Pin;
    request.Property.Id = KSPROPERTY_PIN_GLOBALCINSTANCES;
    request.Property.Flags = KSPROPERTY_TYPE_GET;
    request.PinId = connect->PinId;
    request.Reserved = 0;

    KSPIN_CINSTANCES instances{};
    DWORD returned = 0;

    if (!DeviceIoControl(
            filter_handle,
            IOCTL_KS_PROPERTY,
            &request,
            sizeof(request),
            &instances,
            sizeof(instances),
            &returned,
            nullptr)) {
        const DWORD error = GetLastError();
        std::printf(
            "GLOBAL INSTANCE GATE: query FAILED PinId=%lu Win32=%lu; KsCreatePin SKIPPED (fail closed)\n",
            connect->PinId,
            error);
        std::fflush(stdout);
        return static_cast<NTSTATUS>(error ? error : ERROR_GEN_FAILURE);
    }

    const bool busy = instances.CurrentCount >= instances.PossibleCount;
    std::printf(
        "GLOBAL INSTANCE GATE: PinId=%lu PossibleCount=%lu CurrentCount=%lu busy=%s\n",
        connect->PinId,
        instances.PossibleCount,
        instances.CurrentCount,
        busy ? "YES" : "NO");
    std::fflush(stdout);

    if (busy) {
        std::printf("GLOBAL INSTANCE GATE: BUSY -> KsCreatePin SKIPPED\n");
        std::fflush(stdout);
        SetLastError(ERROR_BUSY);
        return static_cast<NTSTATUS>(ERROR_BUSY);
    }

    std::printf("GLOBAL INSTANCE GATE: FREE -> calling real KsCreatePin\n");
    std::fflush(stdout);
    return KsCreatePin(filter_handle, connect, desired_access, connection_handle);
}

// This macro is intentionally defined only after X4GuardedKsCreatePin has been
// parsed, so the wrapper's final call above still binds to the real SDK API.
#define KsCreatePin X4GuardedKsCreatePin
