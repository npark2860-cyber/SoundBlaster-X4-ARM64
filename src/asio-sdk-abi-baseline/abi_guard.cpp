#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <ks.h>
#include <mmreg.h>
#include <ksmedia.h>

#include <cstddef>

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error ABI guard must be compiled as native Windows ARM64.
#endif

// Compile-time comparison between the Microsoft Windows SDK ARM64 ABI and the
// hand-declared ABI used by the earlier Stage A/A0 probes. If any assertion
// fails, do not run the executable on hardware; fix the ABI mismatch first.

static_assert(sizeof(void*) == 8, "ARM64 pointer size mismatch");
static_assert(alignof(void*) == 8, "ARM64 pointer alignment mismatch");

static_assert(sizeof(GUID) == 16, "GUID size differs from prior probe ABI");

static_assert(sizeof(SP_DEVICE_INTERFACE_DATA) == 32,
              "SP_DEVICE_INTERFACE_DATA size differs from prior probe ABI");
static_assert(offsetof(SP_DEVICE_INTERFACE_DATA, cbSize) == 0,
              "SP_DEVICE_INTERFACE_DATA.cbSize offset mismatch");
static_assert(offsetof(SP_DEVICE_INTERFACE_DATA, InterfaceClassGuid) == 4,
              "SP_DEVICE_INTERFACE_DATA.InterfaceClassGuid offset mismatch");
static_assert(offsetof(SP_DEVICE_INTERFACE_DATA, Flags) == 20,
              "SP_DEVICE_INTERFACE_DATA.Flags offset mismatch");
static_assert(offsetof(SP_DEVICE_INTERFACE_DATA, Reserved) == 24,
              "SP_DEVICE_INTERFACE_DATA.Reserved offset mismatch");

// Earlier probes used cbSize=8 while reading DevicePath at byte offset 4.
static_assert(sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) == 8,
              "SP_DEVICE_INTERFACE_DETAIL_DATA_W size differs from prior probe ABI");
static_assert(offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA_W, cbSize) == 0,
              "SP_DEVICE_INTERFACE_DETAIL_DATA_W.cbSize offset mismatch");
static_assert(offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath) == 4,
              "SP_DEVICE_INTERFACE_DETAIL_DATA_W.DevicePath offset mismatch");

static_assert(sizeof(KSPROPERTY) == 24, "KSPROPERTY size differs from prior probe ABI");
static_assert(offsetof(KSPROPERTY, Set) == 0, "KSPROPERTY.Set offset mismatch");
static_assert(offsetof(KSPROPERTY, Id) == 16, "KSPROPERTY.Id offset mismatch");
static_assert(offsetof(KSPROPERTY, Flags) == 20, "KSPROPERTY.Flags offset mismatch");

static_assert(sizeof(KSPIN_INTERFACE) == 24, "KSPIN_INTERFACE size mismatch");
static_assert(sizeof(KSPIN_MEDIUM) == 24, "KSPIN_MEDIUM size mismatch");
static_assert(sizeof(KSPRIORITY) == 8, "KSPRIORITY size mismatch");

static_assert(sizeof(KSPIN_CONNECT) == 72,
              "KSPIN_CONNECT size differs from prior probe ABI");
static_assert(offsetof(KSPIN_CONNECT, Interface) == 0,
              "KSPIN_CONNECT.Interface offset mismatch");
static_assert(offsetof(KSPIN_CONNECT, Medium) == 24,
              "KSPIN_CONNECT.Medium offset mismatch");
static_assert(offsetof(KSPIN_CONNECT, PinId) == 48,
              "KSPIN_CONNECT.PinId offset mismatch");
static_assert(offsetof(KSPIN_CONNECT, PinToHandle) == 56,
              "KSPIN_CONNECT.PinToHandle offset mismatch");
static_assert(offsetof(KSPIN_CONNECT, Priority) == 64,
              "KSPIN_CONNECT.Priority offset mismatch");

static_assert(sizeof(KSDATAFORMAT) == 64, "KSDATAFORMAT size mismatch");
static_assert(sizeof(WAVEFORMATEX) == 18, "WAVEFORMATEX size mismatch");
static_assert(sizeof(WAVEFORMATEXTENSIBLE) == 40, "WAVEFORMATEXTENSIBLE size mismatch");
static_assert(sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE) == 104,
              "KSDATAFORMAT_WAVEFORMATEXTENSIBLE size differs from prior probe request");
static_assert(offsetof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE, DataFormat) == 0,
              "KSDATAFORMAT_WAVEFORMATEXTENSIBLE.DataFormat offset mismatch");
static_assert(offsetof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE, WaveFormatExt) == 64,
              "KSDATAFORMAT_WAVEFORMATEXTENSIBLE.WaveFormatExt offset mismatch");

static_assert(sizeof(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION) == 40,
              "KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION size differs from prior probe ABI");
static_assert(offsetof(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION, Property) == 0,
              "WaveRT notification buffer Property offset mismatch");
static_assert(offsetof(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION, BaseAddress) == 24,
              "WaveRT notification buffer BaseAddress offset mismatch");
static_assert(offsetof(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION, RequestedBufferSize) == 32,
              "WaveRT notification buffer RequestedBufferSize offset mismatch");
static_assert(offsetof(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION, NotificationCount) == 36,
              "WaveRT notification buffer NotificationCount offset mismatch");

static_assert(sizeof(KSRTAUDIO_BUFFER) == 16,
              "KSRTAUDIO_BUFFER size differs from prior probe ABI");
static_assert(offsetof(KSRTAUDIO_BUFFER, BufferAddress) == 0,
              "KSRTAUDIO_BUFFER.BufferAddress offset mismatch");
static_assert(offsetof(KSRTAUDIO_BUFFER, ActualBufferSize) == 8,
              "KSRTAUDIO_BUFFER.ActualBufferSize offset mismatch");
static_assert(offsetof(KSRTAUDIO_BUFFER, CallMemoryBarrier) == 12,
              "KSRTAUDIO_BUFFER.CallMemoryBarrier offset mismatch");

static_assert(sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY) == 32,
              "KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY size differs from prior probe ABI");
static_assert(offsetof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY, Property) == 0,
              "WaveRT notification event Property offset mismatch");
static_assert(offsetof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY, NotificationEvent) == 24,
              "WaveRT notification event handle offset mismatch");

static_assert(sizeof(KSAUDIO_PRESENTATION_POSITION) == 16,
              "KSAUDIO_PRESENTATION_POSITION size differs from prior probe ABI");
static_assert(offsetof(KSAUDIO_PRESENTATION_POSITION, u64PositionInBlocks) == 0,
              "KSAUDIO_PRESENTATION_POSITION position offset mismatch");
static_assert(offsetof(KSAUDIO_PRESENTATION_POSITION, u64QPCPosition) == 8,
              "KSAUDIO_PRESENTATION_POSITION QPC offset mismatch");
