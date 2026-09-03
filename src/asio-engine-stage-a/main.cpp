// Sound Blaster X4 Windows ARM64 - ASIO Engine Stage A
// Independent native prototype. Creative binaries are reference-only and are not loaded.
// Narrow scope: X4 msft_wave / Render Pin 1 / 48 kHz / stereo / 16-bit PCM /
// 4096-byte WaveRT buffer / NotificationCount=2 / 512-frame logical buffers.

// This file intentionally declares only the Win32/KS ABI surface used by Stage A.
// It can be built with the Windows SDK/MSVC or cross-linked as a freestanding ARM64 PE.

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef long long          i64;
typedef int                BOOL;
typedef void*              HANDLE;
typedef void*              HDEVINFO;
typedef void*              HMODULE;
typedef void*              HWND;
typedef void*              LPVOID;
typedef const void*        LPCVOID;
typedef unsigned long long ULONG_PTR;

struct GUID {
    u32 Data1;
    u16 Data2;
    u16 Data3;
    u8  Data4[8];
};

struct SP_DEVICE_INTERFACE_DATA_MIN {
    u32 cbSize;
    GUID InterfaceClassGuid;
    u32 Flags;
    ULONG_PTR Reserved;
};

struct KSPROPERTY_MIN {
    GUID Set;
    u32 Id;
    u32 Flags;
};

struct KSPIN_INTERFACE_MIN {
    GUID Set;
    u32 Id;
    u32 Flags;
};

struct KSPIN_MEDIUM_MIN {
    GUID Set;
    u32 Id;
    u32 Flags;
};

struct KSPRIORITY_MIN {
    u32 PriorityClass;
    u32 PrioritySubClass;
};

struct KSPIN_CONNECT_MIN {
    KSPIN_INTERFACE_MIN Interface;
    KSPIN_MEDIUM_MIN Medium;
    u32 PinId;
    HANDLE PinToHandle;
    KSPRIORITY_MIN Priority;
};

struct KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION_MIN {
    KSPROPERTY_MIN Property;
    void* BaseAddress;
    u32 RequestedBufferSize;
    u32 NotificationCount;
};

struct KSRTAUDIO_BUFFER_MIN {
    void* BufferAddress;
    u32 ActualBufferSize;
    BOOL CallMemoryBarrier;
};

struct KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY_MIN {
    KSPROPERTY_MIN Property;
    HANDLE NotificationEvent;
};

struct KSAUDIO_PRESENTATION_POSITION_MIN {
    u64 PositionInBlocks;
    u64 QpcPosition;
};

static_assert(sizeof(GUID) == 16, "GUID ABI");
static_assert(sizeof(SP_DEVICE_INTERFACE_DATA_MIN) == 32, "SP_DEVICE_INTERFACE_DATA ABI");
static_assert(sizeof(KSPROPERTY_MIN) == 24, "KSPROPERTY ABI");
static_assert(sizeof(KSPIN_CONNECT_MIN) == 72, "KSPIN_CONNECT ABI");
static_assert(sizeof(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION_MIN) == 40, "WaveRT buffer property ABI");
static_assert(sizeof(KSRTAUDIO_BUFFER_MIN) == 16, "WaveRT buffer result ABI");
static_assert(sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY_MIN) == 32, "WaveRT notification ABI");

extern "C" {
__declspec(dllimport) HDEVINFO __stdcall SetupDiGetClassDevsW(
    const GUID*, const wchar_t*, HWND, u32);
__declspec(dllimport) BOOL __stdcall SetupDiEnumDeviceInterfaces(
    HDEVINFO, void*, const GUID*, u32, SP_DEVICE_INTERFACE_DATA_MIN*);
__declspec(dllimport) BOOL __stdcall SetupDiGetDeviceInterfaceDetailW(
    HDEVINFO, SP_DEVICE_INTERFACE_DATA_MIN*, void*, u32, u32*, void*);
__declspec(dllimport) BOOL __stdcall SetupDiDestroyDeviceInfoList(HDEVINFO);

__declspec(dllimport) HANDLE __stdcall CreateFileW(
    const wchar_t*, u32, u32, void*, u32, u32, HANDLE);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) BOOL __stdcall DeviceIoControl(
    HANDLE, u32, void*, u32, void*, u32, u32*, void*);
__declspec(dllimport) HANDLE __stdcall CreateEventW(void*, BOOL, BOOL, const wchar_t*);
__declspec(dllimport) u32 __stdcall WaitForSingleObject(HANDLE, u32);
__declspec(dllimport) u32 __stdcall GetLastError(void);
__declspec(dllimport) HANDLE __stdcall GetStdHandle(u32);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE, const void*, u32, u32*, void*);
__declspec(dllimport) void __stdcall ExitProcess(u32);

__declspec(dllimport) u32 __stdcall KsCreatePin(
    HANDLE, KSPIN_CONNECT_MIN*, u32, HANDLE*);
}

static const u32 DIGCF_PRESENT = 0x00000002u;
static const u32 DIGCF_DEVICEINTERFACE = 0x00000010u;
static const u32 GENERIC_READ = 0x80000000u;
static const u32 GENERIC_WRITE = 0x40000000u;
static const u32 FILE_SHARE_READ = 0x00000001u;
static const u32 CREATE_ALWAYS = 2u;
static const u32 OPEN_EXISTING = 3u;
static const u32 FILE_ATTRIBUTE_NORMAL = 0x00000080u;
static const u32 STD_OUTPUT_HANDLE_U32 = 0xFFFFFFF5u;
static const u32 WAIT_OBJECT_0 = 0u;
static const u32 WAIT_TIMEOUT = 258u;
static const u32 ERROR_NO_MORE_ITEMS = 259u;
static const u32 IOCTL_KS_PROPERTY = 0x002F0003u;
static const u32 KSPROPERTY_TYPE_GET = 0x00000001u;
static const u32 KSPROPERTY_TYPE_SET = 0x00000002u;
static const u32 KSPROPERTY_CONNECTION_STATE = 0u;
static const u32 KSSTATE_STOP = 0u;
static const u32 KSSTATE_ACQUIRE = 1u;
static const u32 KSSTATE_PAUSE = 2u;
static const u32 KSSTATE_RUN = 3u;
static const u32 KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION = 5u;
static const u32 KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT = 6u;
static const u32 KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT = 7u;
static const u32 KSPROPERTY_RTAUDIO_PACKETCOUNT = 9u;
static const u32 KSPROPERTY_RTAUDIO_PRESENTATION_POSITION = 10u;
static const u32 KSPRIORITY_NORMAL = 0x40000000u;

static const u32 kSampleRate = 48000u;
static const u32 kChannels = 2u;
static const u32 kBitsPerSample = 16u;
static const u32 kBytesPerFrame = 4u;
static const u32 kRequestedBufferBytes = 4096u;
static const u32 kNotificationCount = 2u;
static const u32 kFramesPerLogicalBuffer = 512u;
static const u32 kLogicalBufferBytes = 2048u;
static const u32 kRuns = 3u;
static const u32 kNotificationsPerRun = 64u;

static const GUID KSCATEGORY_AUDIO =
    {0x6994AD04u,0x93EFu,0x11D0u,{0xA3,0xCC,0x00,0xA0,0xC9,0x22,0x31,0x96}};
static const GUID KSPROPSETID_Connection =
    {0x1D58C920u,0xAC9Bu,0x11CFu,{0xA5,0xD6,0x28,0xDB,0x04,0xC1,0x00,0x00}};
static const GUID KSPROPSETID_RtAudio =
    {0xA855A48Cu,0x2F78u,0x4729u,{0x90,0x51,0x19,0x68,0x74,0x6B,0x9E,0xEF}};
static const GUID KSINTERFACESETID_Standard =
    {0x1A8766A0u,0x62CEu,0x11CFu,{0xA5,0xD6,0x28,0xDB,0x04,0xC1,0x00,0x00}};
static const GUID KSMEDIUMSETID_Standard =
    {0x4747B320u,0x62CEu,0x11CFu,{0xA5,0xD6,0x28,0xDB,0x04,0xC1,0x00,0x00}};
static const GUID KSDATAFORMAT_TYPE_AUDIO =
    {0x73647561u,0x0000u,0x0010u,{0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}};
static const GUID KSDATAFORMAT_SUBTYPE_PCM =
    {0x00000001u,0x0000u,0x0010u,{0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}};
static const GUID KSDATAFORMAT_SPECIFIER_WAVEFORMATEX =
    {0x05589F81u,0xC356u,0x11CEu,{0xBF,0x01,0x00,0xAA,0x00,0x55,0x59,0x5A}};

static HANDLE g_console = (HANDLE)0;
static HANDLE g_log = (HANDLE)0;
static u8 g_detail_buffer[4096];
static u8 g_connect_blob[256];

static void zero_bytes(void* p, u32 n) {
    volatile u8* b = (volatile u8*)p;
    for (u32 i = 0; i < n; ++i) b[i] = 0;
}

static void copy_bytes(void* dst, const void* src, u32 n) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    for (u32 i = 0; i < n; ++i) d[i] = s[i];
}

static u32 str_len(const char* s) {
    u32 n = 0;
    while (s && s[n]) ++n;
    return n;
}

static u32 append_str(char* out, u32 at, u32 cap, const char* s) {
    while (s && *s && at + 1 < cap) out[at++] = *s++;
    out[at] = 0;
    return at;
}

static u32 append_u64(char* out, u32 at, u32 cap, u64 v) {
    char tmp[32];
    u32 n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v && n < 31) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n && at + 1 < cap) out[at++] = tmp[--n];
    out[at] = 0;
    return at;
}

static u32 append_hex32(char* out, u32 at, u32 cap, u32 v) {
    const char* h = "0123456789ABCDEF";
    at = append_str(out, at, cap, "0x");
    for (int shift = 28; shift >= 0 && at + 1 < cap; shift -= 4)
        out[at++] = h[(v >> shift) & 0xFu];
    out[at] = 0;
    return at;
}

static void write_handle(HANDLE h, const char* s) {
    if (!h || h == (HANDLE)(ULONG_PTR)(~0ull)) return;
    u32 n = 0;
    WriteFile(h, s, str_len(s), &n, (void*)0);
}

static void log_line(const char* s) {
    write_handle(g_console, s);
    write_handle(g_console, "\r\n");
    write_handle(g_log, s);
    write_handle(g_log, "\r\n");
}

static void log_key_u64(const char* key, u64 value) {
    char line[256]; line[0] = 0;
    u32 at = append_str(line, 0, 256, key);
    at = append_u64(line, at, 256, value);
    (void)at;
    log_line(line);
}

static void log_error(const char* where, u32 error) {
    char line[256]; line[0] = 0;
    u32 at = append_str(line, 0, 256, where);
    at = append_str(line, at, 256, " failed, Win32=");
    at = append_u64(line, at, 256, error);
    (void)at;
    log_line(line);
}

static u8 fold_ascii_w(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return (u8)(c - L'A' + L'a');
    if (c >= 0 && c <= 127) return (u8)c;
    return 0;
}

static BOOL contains_ascii_i(const wchar_t* hay, const char* needle) {
    if (!hay || !needle || !*needle) return 0;
    for (u32 i = 0; hay[i]; ++i) {
        u32 j = 0;
        while (needle[j] && hay[i + j] && fold_ascii_w(hay[i + j]) == (u8)needle[j]) ++j;
        if (!needle[j]) return 1;
    }
    return 0;
}

static BOOL find_x4_wave_path(wchar_t* out, u32 out_chars) {
    HDEVINFO set = SetupDiGetClassDevsW(&KSCATEGORY_AUDIO, (const wchar_t*)0, (HWND)0,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == (HDEVINFO)(ULONG_PTR)(~0ull)) {
        log_error("SetupDiGetClassDevsW", GetLastError());
        return 0;
    }

    BOOL found = 0;
    for (u32 index = 0; ; ++index) {
        SP_DEVICE_INTERFACE_DATA_MIN ifd;
        zero_bytes(&ifd, (u32)sizeof(ifd));
        ifd.cbSize = (u32)sizeof(ifd);

        if (!SetupDiEnumDeviceInterfaces(set, (void*)0, &KSCATEGORY_AUDIO, index, &ifd)) {
            u32 e = GetLastError();
            if (e == ERROR_NO_MORE_ITEMS) break;
            continue;
        }

        u32 required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifd, (void*)0, 0, &required, (void*)0);
        if (required < 8 || required > (u32)sizeof(g_detail_buffer)) continue;

        zero_bytes(g_detail_buffer, required);
        *(u32*)g_detail_buffer = 8u;
        if (!SetupDiGetDeviceInterfaceDetailW(set, &ifd, g_detail_buffer,
            (u32)sizeof(g_detail_buffer), &required, (void*)0)) continue;

        const wchar_t* path = (const wchar_t*)(g_detail_buffer + 4);
        if (contains_ascii_i(path, "vid_041e&pid_3278&mi_03") &&
            contains_ascii_i(path, "\\msft_wave")) {
            u32 i = 0;
            while (path[i] && i + 1 < out_chars) { out[i] = path[i]; ++i; }
            out[i] = 0;
            found = 1;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(set);
    return found;
}

static void write_u16(u8* b, u32 off, u16 v) {
    b[off+0] = (u8)(v & 0xFFu); b[off+1] = (u8)((v >> 8) & 0xFFu);
}
static void write_u32(u8* b, u32 off, u32 v) {
    b[off+0]=(u8)(v); b[off+1]=(u8)(v>>8); b[off+2]=(u8)(v>>16); b[off+3]=(u8)(v>>24);
}
static void write_guid(u8* b, u32 off, const GUID* g) { copy_bytes(b + off, g, 16); }

static KSPROPERTY_MIN make_property(const GUID* set, u32 id, u32 flags) {
    KSPROPERTY_MIN p; zero_bytes(&p, (u32)sizeof(p)); p.Set=*set; p.Id=id; p.Flags=flags; return p;
}

static u32 build_pin_connect(void) {
    zero_bytes(g_connect_blob, (u32)sizeof(g_connect_blob));
    KSPIN_CONNECT_MIN c; zero_bytes(&c, (u32)sizeof(c));
    c.Interface.Set = KSINTERFACESETID_Standard;
    c.Interface.Id = 1;
    c.Medium.Set = KSMEDIUMSETID_Standard;
    c.Medium.Id = 0;
    c.PinId = 1;
    c.Priority.PriorityClass = KSPRIORITY_NORMAL;
    c.Priority.PrioritySubClass = 1;
    copy_bytes(g_connect_blob, &c, (u32)sizeof(c));

    u8* b = g_connect_blob + sizeof(c);
    const u32 format_size = 104u;
    write_u32(b, 0, format_size);
    write_u32(b, 4, 0); write_u32(b, 8, 0); write_u32(b, 12, 0);
    write_guid(b,16,&KSDATAFORMAT_TYPE_AUDIO);
    write_guid(b,32,&KSDATAFORMAT_SUBTYPE_PCM);
    write_guid(b,48,&KSDATAFORMAT_SPECIFIER_WAVEFORMATEX);
    write_u16(b,64,0xFFFEu);
    write_u16(b,66,(u16)kChannels);
    write_u32(b,68,kSampleRate);
    write_u32(b,72,kSampleRate*kBytesPerFrame);
    write_u16(b,76,(u16)kBytesPerFrame);
    write_u16(b,78,(u16)kBitsPerSample);
    write_u16(b,80,22u);
    write_u16(b,82,(u16)kBitsPerSample);
    write_u32(b,84,0x3u);
    write_guid(b,88,&KSDATAFORMAT_SUBTYPE_PCM);
    return (u32)sizeof(c) + format_size;
}

static BOOL set_state(HANDLE pin, u32 state) {
    KSPROPERTY_MIN p = make_property(&KSPROPSETID_Connection, KSPROPERTY_CONNECTION_STATE, KSPROPERTY_TYPE_SET);
    u32 returned = 0;
    BOOL ok = DeviceIoControl(pin, IOCTL_KS_PROPERTY, &p, (u32)sizeof(p), &state, 4, &returned, (void*)0);
    if (!ok) log_error("KSPROPERTY_CONNECTION_STATE", GetLastError());
    return ok;
}

static BOOL get_packet_count(HANDLE pin, u32* value) {
    KSPROPERTY_MIN p = make_property(&KSPROPSETID_RtAudio, KSPROPERTY_RTAUDIO_PACKETCOUNT, KSPROPERTY_TYPE_GET);
    u32 returned = 0;
    return DeviceIoControl(pin, IOCTL_KS_PROPERTY, &p, (u32)sizeof(p), value, 4, &returned, (void*)0);
}

static BOOL get_presentation_position(HANDLE pin, KSAUDIO_PRESENTATION_POSITION_MIN* value) {
    KSPROPERTY_MIN p = make_property(&KSPROPSETID_RtAudio, KSPROPERTY_RTAUDIO_PRESENTATION_POSITION, KSPROPERTY_TYPE_GET);
    u32 returned = 0;
    return DeviceIoControl(pin, IOCTL_KS_PROPERTY, &p, (u32)sizeof(p), value, (u32)sizeof(*value), &returned, (void*)0);
}

static void silence_callback(void* logical_buffer, u32 frames) {
    volatile u8* p = (volatile u8*)logical_buffer;
    u32 bytes = frames * kBytesPerFrame;
    for (u32 i = 0; i < bytes; ++i) p[i] = 0;
}

struct RunStats {
    u32 callbacks;
    u32 packet_discontinuities;
    u32 alternation_errors;
    u32 position_regressions;
};

static BOOL run_once(u32 run_index, RunStats* stats) {
    wchar_t path[1024]; zero_bytes(path, (u32)sizeof(path));
    if (!find_x4_wave_path(path, 1024)) {
        log_line("X4 msft_wave filter not found.");
        return 0;
    }

    HANDLE filter = CreateFileW(path, GENERIC_READ|GENERIC_WRITE, 0, (void*)0, OPEN_EXISTING, 0, (HANDLE)0);
    if (filter == (HANDLE)(ULONG_PTR)(~0ull)) {
        log_error("CreateFileW(msft_wave)", GetLastError());
        return 0;
    }

    build_pin_connect();
    HANDLE pin = (HANDLE)0;
    u32 status = KsCreatePin(filter, (KSPIN_CONNECT_MIN*)g_connect_blob, GENERIC_WRITE, &pin);
    if (status != 0 || !pin || pin == (HANDLE)(ULONG_PTR)(~0ull)) {
        char line[128]; line[0]=0; u32 at=append_str(line,0,128,"KsCreatePin status="); append_hex32(line,at,128,status); log_line(line);
        CloseHandle(filter);
        return 0;
    }

    KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION_MIN req;
    zero_bytes(&req, (u32)sizeof(req));
    req.Property = make_property(&KSPROPSETID_RtAudio, KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION, KSPROPERTY_TYPE_GET);
    req.RequestedBufferSize = kRequestedBufferBytes;
    req.NotificationCount = kNotificationCount;
    KSRTAUDIO_BUFFER_MIN buffer; zero_bytes(&buffer, (u32)sizeof(buffer));
    u32 returned = 0;
    if (!DeviceIoControl(pin, IOCTL_KS_PROPERTY, &req, (u32)sizeof(req), &buffer, (u32)sizeof(buffer), &returned, (void*)0)) {
        log_error("BUFFER_WITH_NOTIFICATION", GetLastError()); CloseHandle(pin); CloseHandle(filter); return 0;
    }

    if (!buffer.BufferAddress || buffer.ActualBufferSize != kRequestedBufferBytes) {
        log_line("Unexpected WaveRT buffer geometry."); CloseHandle(pin); CloseHandle(filter); return 0;
    }
    if (buffer.CallMemoryBarrier) {
        log_line("CallMemoryBarrier=1 is outside hardware-confirmed Stage A path; stopping."); CloseHandle(pin); CloseHandle(filter); return 0;
    }

    zero_bytes(buffer.BufferAddress, buffer.ActualBufferSize);
    HANDLE evt = CreateEventW((void*)0, 0, 0, (const wchar_t*)0);
    if (!evt) { log_error("CreateEventW", GetLastError()); CloseHandle(pin); CloseHandle(filter); return 0; }

    KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY_MIN ep;
    zero_bytes(&ep, (u32)sizeof(ep));
    ep.Property = make_property(&KSPROPSETID_RtAudio, KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT, KSPROPERTY_TYPE_SET);
    ep.NotificationEvent = evt;
    if (!DeviceIoControl(pin, IOCTL_KS_PROPERTY, &ep, (u32)sizeof(ep), (void*)0, 0, &returned, (void*)0)) {
        log_error("REGISTER_NOTIFICATION_EVENT", GetLastError()); CloseHandle(evt); CloseHandle(pin); CloseHandle(filter); return 0;
    }

    BOOL running = 0;
    BOOL ok = 0;
    if (!set_state(pin, KSSTATE_ACQUIRE)) goto cleanup;
    if (!set_state(pin, KSSTATE_PAUSE)) goto cleanup;
    if (!set_state(pin, KSSTATE_RUN)) goto cleanup;
    running = 1;

    {
        u32 prev_packet = 0;
        u32 prev_index = 0;
        BOOL have_prev_index = 0;
        u64 prev_position = 0;
        BOOL have_prev_position = 0;

        for (u32 i = 0; i < kNotificationsPerRun; ++i) {
            u32 wr = WaitForSingleObject(evt, 500);
            if (wr != WAIT_OBJECT_0) {
                if (wr == WAIT_TIMEOUT) log_line("DMA notification timeout.");
                else log_error("WaitForSingleObject", GetLastError());
                goto cleanup;
            }

            u32 packet = 0;
            KSAUDIO_PRESENTATION_POSITION_MIN pos; zero_bytes(&pos, (u32)sizeof(pos));
            if (!get_packet_count(pin, &packet)) { log_error("PACKETCOUNT", GetLastError()); goto cleanup; }
            if (!get_presentation_position(pin, &pos)) { log_error("PRESENTATION_POSITION", GetLastError()); goto cleanup; }

            if (prev_packet && packet != prev_packet + 1) ++stats->packet_discontinuities;
            prev_packet = packet;

            u32 index = packet ? ((packet - 1u) & 1u) : (i & 1u);
            if (have_prev_index && index == prev_index) ++stats->alternation_errors;
            prev_index = index; have_prev_index = 1;

            if (have_prev_position && pos.PositionInBlocks < prev_position) ++stats->position_regressions;
            prev_position = pos.PositionInBlocks; have_prev_position = 1;

            u8* logical = (u8*)buffer.BufferAddress + index * kLogicalBufferBytes;
            silence_callback(logical, kFramesPerLogicalBuffer);
            ++stats->callbacks;

            char line[320]; line[0]=0; u32 at=0;
            at=append_str(line,at,320,"run="); at=append_u64(line,at,320,run_index);
            at=append_str(line,at,320," callback="); at=append_u64(line,at,320,i+1);
            at=append_str(line,at,320," buffer="); at=append_u64(line,at,320,index);
            at=append_str(line,at,320," packet="); at=append_u64(line,at,320,packet);
            at=append_str(line,at,320," samplePosition="); at=append_u64(line,at,320,pos.PositionInBlocks);
            at=append_str(line,at,320," qpc="); at=append_u64(line,at,320,pos.QpcPosition);
            (void)at; log_line(line);
        }
    }

    ok = 1;

cleanup:
    if (running) {
        set_state(pin, KSSTATE_PAUSE);
        set_state(pin, KSSTATE_ACQUIRE);
    }
    set_state(pin, KSSTATE_STOP);

    ep.Property = make_property(&KSPROPSETID_RtAudio, KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT, KSPROPERTY_TYPE_SET);
    DeviceIoControl(pin, IOCTL_KS_PROPERTY, &ep, (u32)sizeof(ep), (void*)0, 0, &returned, (void*)0);
    CloseHandle(evt);
    CloseHandle(pin);
    CloseHandle(filter);
    return ok;
}

static u32 program_main(void) {
    g_console = GetStdHandle(STD_OUTPUT_HANDLE_U32);
    g_log = CreateFileW(L"x4-asio-engine-stage-a.txt", GENERIC_WRITE, FILE_SHARE_READ,
        (void*)0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, (HANDLE)0);

    log_line("Sound Blaster X4 native ARM64 ASIO Engine Stage A");
    log_line("Scope: Render Pin 1 / 48kHz / stereo / 16-bit / 4096-byte WaveRT / 2 notifications / 512-frame logical buffers");
    log_line("Creative runtime dependencies: NONE");

    RunStats stats; zero_bytes(&stats, (u32)sizeof(stats));
    for (u32 run = 1; run <= kRuns; ++run) {
        char line[96]; line[0]=0; u32 at=append_str(line,0,96,"=== RUN "); at=append_u64(line,at,96,run); append_str(line,at,96,"/3 ==="); log_line(line);
        if (!run_once(run, &stats)) {
            log_line("STAGE A RESULT: FAIL");
            if (g_log && g_log != (HANDLE)(ULONG_PTR)(~0ull)) CloseHandle(g_log);
            return 2;
        }
    }

    log_key_u64("callbacks=", stats.callbacks);
    log_key_u64("packet_discontinuities=", stats.packet_discontinuities);
    log_key_u64("alternation_errors=", stats.alternation_errors);
    log_key_u64("position_regressions=", stats.position_regressions);

    BOOL pass = (stats.callbacks == kRuns*kNotificationsPerRun &&
                 stats.packet_discontinuities == 0 &&
                 stats.alternation_errors == 0 &&
                 stats.position_regressions == 0);
    log_line(pass ? "STAGE A RESULT: PASS" : "STAGE A RESULT: FAIL (invariant violation)");

    if (g_log && g_log != (HANDLE)(ULONG_PTR)(~0ull)) CloseHandle(g_log);
    return pass ? 0u : 3u;
}

#ifdef X4_FREESTANDING
extern "C" void mainCRTStartup(void) {
    ExitProcess(program_main());
}
#else
int main(void) {
    return (int)program_main();
}
#endif
