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

// ASIO host callback message selectors used by Stage B4C.
constexpr long kAsioSelectorSupported = 1;
constexpr long kAsioEngineVersion = 2;
constexpr long kAsioResetRequest = 3;
constexpr long kAsioBufferSizeChange = 4;
constexpr long kAsioResyncRequest = 5;
constexpr long kAsioLatenciesChanged = 6;
constexpr long kAsioSupportsTimeInfo = 7;
constexpr long kAsioSupportsTimeCode = 8;

// ASIOFuture selector sequence from the ASIO 2.x public interface.
constexpr long kAsioEnableTimeCodeRead = 1;
constexpr long kAsioDisableTimeCodeRead = 2;
constexpr long kAsioSetInputMonitor = 3;
constexpr long kAsioTransport = 4;
constexpr long kAsioSetInputGain = 5;
constexpr long kAsioGetInputMeter = 6;
constexpr long kAsioSetOutputGain = 7;
constexpr long kAsioGetOutputMeter = 8;
constexpr long kAsioCanInputMonitor = 9;
constexpr long kAsioCanTimeInfo = 10;
constexpr long kAsioCanTimeCode = 11;

constexpr unsigned long kSystemTimeValid = 1u;
constexpr unsigned long kSamplePositionValid = 1u << 1;
constexpr unsigned long kSampleRateValid = 1u << 2;
constexpr unsigned long kSpeedValid = 1u << 3;
constexpr unsigned long kSampleRateChanged = 1u << 4;
constexpr unsigned long kClockSourceChanged = 1u << 5;

constexpr unsigned long kTcValid = 1u;
constexpr unsigned long kTcRunning = 1u << 1;
constexpr unsigned long kTcReverse = 1u << 2;
constexpr unsigned long kTcOnspeed = 1u << 3;
constexpr unsigned long kTcStill = 1u << 4;
constexpr unsigned long kTcSpeedValid = 1u << 8;

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

struct AsioTimeInfo {
    double speed;
    ASIOTimeStamp systemTime;
    ASIOSamples samplePosition;
    ASIOSampleRate sampleRate;
    unsigned long flags;
    char reserved[12];
};

struct ASIOTimeCode {
    double speed;
    ASIOSamples timeCodeSamples;
    unsigned long flags;
    char future[64];
};

struct ASIOTime {
    long reserved[4];
    AsioTimeInfo timeInfo;
    ASIOTimeCode timeCode;
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
static_assert(sizeof(AsioTimeInfo) == 48, "Unexpected AsioTimeInfo ABI size");
static_assert(alignof(AsioTimeInfo) == 4, "AsioTimeInfo must use 4-byte packing");
static_assert(sizeof(ASIOTimeCode) == 84, "Unexpected ASIOTimeCode ABI size");
static_assert(alignof(ASIOTimeCode) == 4, "ASIOTimeCode must use 4-byte packing");
static_assert(sizeof(ASIOTime) == 148, "Unexpected ASIOTime ABI size");
static_assert(alignof(ASIOTime) == 4, "ASIOTime must use 4-byte packing");
static_assert(offsetof(ASIOTime, timeInfo) == 16, "Unexpected ASIOTime::timeInfo offset");
static_assert(offsetof(ASIOTime, timeCode) == 64, "Unexpected ASIOTime::timeCode offset");
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
