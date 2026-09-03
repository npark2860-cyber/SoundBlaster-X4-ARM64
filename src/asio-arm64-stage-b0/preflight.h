#pragma once

#include <windows.h>

struct X4InstancePreflightResult {
    bool device_found = false;
    bool filter_opened = false;
    bool local_ok = false;
    bool global_ok = false;

    DWORD open_error = ERROR_SUCCESS;
    DWORD local_error = ERROR_SUCCESS;
    DWORD global_error = ERROR_SUCCESS;

    ULONG local_possible = 0;
    ULONG local_current = 0;
    ULONG global_possible = 0;
    ULONG global_current = 0;
};

X4InstancePreflightResult run_x4_instance_preflight();
