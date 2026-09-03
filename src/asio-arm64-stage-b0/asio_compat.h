#pragma once

#include <windows.h>
#include <unknwn.h>
#include <cstddef>

using ASIOBool = long;
using ASIOError = long;
using ASIOSampleRate = double;
using ASIOSamples = long long;
using ASIOTimeStamp = long long;
using ASIOSampleType = long;

constexpr ASIOBool ASIOFalse = 0;
constexpr ASIOBool ASIOTrue = 1;

constexpr ASIOError ASE_OK = 0;
constexpr ASIOError ASE_SUCCESS = 0x3f4847a0L;
constexpr ASIOError ASE_NotPresent = -1000;
constexpr ASIOError ASE_HWMalfunction = -999;
constexpr ASIOError ASE_InvalidParameter = -998;
constexpr ASIOError ASE_InvalidMode = -997;
constexpr ASIOError ASE_SPNotAdvancing = -996;
constexpr ASIOError ASE_NoClock = -995;
constexpr ASIOError ASE_NoMemory = -994;

constexpr ASIOSampleType ASIOSTInt16LSB = 16;

#pragma pack(push, 4)
struct ASIOClockSource {
    long index;
    long associatedChannel;
    long associatedGroup;
    ASIOBool isCurrentSource;
    char name[32];
};

struct ASIOChannelInfo {
    long channel;
    ASIOBool isInput;
    ASIOBool isActive;
    long channelGroup;
    ASIOSampleType type;
    char name[32];
};
#pragma pack(pop)

struct ASIOBufferInfo;
struct ASIOCallbacks;

#if defined(_M_ARM64) && !defined(_M_ARM64EC)
static_assert(sizeof(long) == 4, "Windows ASIO ABI requires 32-bit long");
static_assert(sizeof(ASIOSamples) == 8, "Windows ASIO sample counter must be 64-bit");
static_assert(sizeof(ASIOTimeStamp) == 8, "Windows ASIO timestamp must be 64-bit");
static_assert(sizeof(ASIOClockSource) == 48, "Unexpected ASIOClockSource ABI size");
static_assert(alignof(ASIOClockSource) == 4, "ASIOClockSource must use 4-byte packing");
static_assert(sizeof(ASIOChannelInfo) == 52, "Unexpected ASIOChannelInfo ABI size");
static_assert(alignof(ASIOChannelInfo) == 4, "ASIOChannelInfo must use 4-byte packing");
#endif

// Minimal ABI-compatible ASIO interface declaration used by this independent
// ARM64 implementation. On Windows, ASIO hosts instantiate the COM class and
// call this vtable directly.
struct IASIO : IUnknown {
    virtual ASIOBool init(void* sysHandle) = 0;
    virtual void getDriverName(char* name) = 0;
    virtual long getDriverVersion() = 0;
    virtual void getErrorMessage(char* string) = 0;
    virtual ASIOError start() = 0;
    virtual ASIOError stop() = 0;
    virtual ASIOError getChannels(long* numInputChannels, long* numOutputChannels) = 0;
    virtual ASIOError getLatencies(long* inputLatency, long* outputLatency) = 0;
    virtual ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) = 0;
    virtual ASIOError canSampleRate(ASIOSampleRate sampleRate) = 0;
    virtual ASIOError getSampleRate(ASIOSampleRate* sampleRate) = 0;
    virtual ASIOError setSampleRate(ASIOSampleRate sampleRate) = 0;
    virtual ASIOError getClockSources(ASIOClockSource* clocks, long* numSources) = 0;
    virtual ASIOError setClockSource(long reference) = 0;
    virtual ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) = 0;
    virtual ASIOError getChannelInfo(ASIOChannelInfo* info) = 0;
    virtual ASIOError createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) = 0;
    virtual ASIOError disposeBuffers() = 0;
    virtual ASIOError controlPanel() = 0;
    virtual ASIOError future(long selector, void* opt) = 0;
    virtual ASIOError outputReady() = 0;
};

inline constexpr GUID CLSID_X4_ARM64_ASIO =
    {0x0aa6d99c, 0x4af6, 0x45ef, {0x9c, 0xca, 0x10, 0xac, 0x92, 0x39, 0xb7, 0xd4}};

inline constexpr wchar_t kX4AsioClsidString[] = L"{0AA6D99C-4AF6-45EF-9CCA-10AC9239B7D4}";
inline constexpr wchar_t kX4AsioRegistryName[] = L"Sound Blaster X4 ARM64 ASIO";
inline constexpr wchar_t kX4AsioDescription[] = L"Sound Blaster X4 native ARM64 ASIO";
inline constexpr char kX4AsioDriverName[] = "Sound Blaster X4 ARM64";
