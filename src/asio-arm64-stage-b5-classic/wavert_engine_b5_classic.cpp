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

#include "../asio-arm64-stage-b0/wavert_engine_b5.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error B5 Classic WaveRT adapter must be built for native Windows ARM64.
#endif

#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof((array)[0]))
#endif
#define PerformanceCount PerformanceCounterValue

#include "../asio-arm64-stage-b0/wavert_engine_b5.cpp"
