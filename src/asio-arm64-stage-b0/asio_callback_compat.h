#pragma once

#include <cstddef>

#include "asio_compat.h"

// Minimal concrete ASIO callback/buffer ABI used by Stage B3A.
// These complete the forward declarations in asio_compat.h without importing
// or depending on Creative runtime components.

struct ASIOTime;

struct ASIOBufferInfo {
    ASIOBool isInput;
    long channelNum;
    void* buffers[2];
};

using ASIOBufferSwitchProc = void (*)(long doubleBufferIndex, ASIOBool directProcess);
using ASIOSampleRateDidChangeProc = void (*)(ASIOSampleRate sRate);
using ASIOMessageProc = long (*)(long selector, long value, void* message, double* opt);
using ASIOBufferSwitchTimeInfoProc = ASIOTime* (*)(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess);

struct ASIOCallbacks {
    ASIOBufferSwitchProc bufferSwitch;
    ASIOSampleRateDidChangeProc sampleRateDidChange;
    ASIOMessageProc asioMessage;
    ASIOBufferSwitchTimeInfoProc bufferSwitchTimeInfo;
};

#if defined(_M_ARM64) && !defined(_M_ARM64EC)
static_assert(sizeof(long) == 4, "Windows ASIO ABI requires 32-bit long");
static_assert(sizeof(ASIOBufferInfo) == 24, "Unexpected ARM64 ASIOBufferInfo size");
static_assert(alignof(ASIOBufferInfo) == 8, "Unexpected ARM64 ASIOBufferInfo alignment");
static_assert(offsetof(ASIOBufferInfo, isInput) == 0, "Unexpected ASIOBufferInfo::isInput offset");
static_assert(offsetof(ASIOBufferInfo, channelNum) == 4, "Unexpected ASIOBufferInfo::channelNum offset");
static_assert(offsetof(ASIOBufferInfo, buffers) == 8, "Unexpected ASIOBufferInfo::buffers offset");
static_assert(sizeof(ASIOCallbacks) == 32, "Unexpected ARM64 ASIOCallbacks size");
static_assert(alignof(ASIOCallbacks) == 8, "Unexpected ARM64 ASIOCallbacks alignment");
#endif
