#define wmain x4_readonly_capability_probe_wmain
#include "main.cpp"
#undef wmain

namespace
{
struct MixerDescriptor
{
    std::uint8_t index{};
    std::uint16_t raw{};
    std::uint8_t type{};
    bool hasVolume{};
    bool hasMute{};
    bool isSource{};
    bool isRecording{};
    bool hasCustomName{};
    bool isDbLevel{};
    std::uint8_t channelNibble{};
};

std::uint16_t u16le(std::vector<std::uint8_t> const& data, std::size_t offset)
{
    return static_cast<std::uint16_t>(data[offset] |
        (static_cast<std::uint16_t>(data[offset + 1]) << 8));
}

std::uint32_t u32le(std::vector<std::uint8_t> const& data, std::size_t offset)
{
    return static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

std::string hex_u16(std::uint16_t value)
{
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setfill('0')
        << std::setw(4) << static_cast<unsigned>(value);
    return out.str();
}

std::string hex_u32(std::uint32_t value)
{
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setfill('0')
        << std::setw(8) << value;
    return out.str();
}

std::string audio_control_type_name(std::uint8_t type)
{
    switch (type)
    {
    case 1: return "Speaker";
    case 2: return "Headphone";
    case 3: return "MicInput";
    case 4: return "LineInput";
    case 5: return "WhatUHearRecording";
    case 6: return "USBInput";
    case 7: return "BluetoothInput";
    case 8: return "RoomCalibration";
    case 9: return "SPDIFInput";
    case 10: return "AuxInput";
    case 11: return "SmartDeviceInput";
    case 12: return "ExternalMicInput";
    case 13: return "Subwoofer";
    case 14: return "SPDIFOutput";
    case 15: return "MicMonitoring";
    case 16: return "LineMonitoring";
    case 17: return "SPDIFMonitoring";
    case 18: return "ChatAudio";
    case 19: return "GameAudio";
    case 20: return "Headset";
    case 63: return "AutomaticGainControl";
    default: return "Unknown(" + std::to_string(type) + ")";
    }
}

std::string ack_status_name(std::uint8_t status)
{
    switch (status)
    {
    case 0x00: return "GeneralSuccess";
    case 0x01: return "DataPending";
    case 0x02: return "ConditionalSuccess";
    case 0x80: return "GeneralFailure";
    case 0x81: return "NotSupported";
    case 0x82: return "TemporarilyUnsupported";
    case 0x83: return "InvalidParameter";
    case 0x84: return "InvalidLength";
    case 0x85: return "DeviceBootingUp";
    default: return "UnknownStatus";
    }
}

Frame const* find_command(std::vector<Frame> const& frames, std::uint8_t command)
{
    auto const it = std::find_if(frames.begin(), frames.end(),
        [command](Frame const& frame) { return frame.command == command; });
    return it == frames.end() ? nullptr : &*it;
}

void log_ack_if_present(Logger& log, std::vector<Frame> const& frames,
    std::uint8_t associatedCommand)
{
    for (auto const& frame : frames)
    {
        if (frame.command == 0x02 && frame.payload.size() >= 2 &&
            frame.payload[0] == associatedCommand)
        {
            log.line("ACK for command 0x" + byte_hex(associatedCommand) +
                ": status=0x" + byte_hex(frame.payload[1]) +
                " (" + ack_status_name(frame.payload[1]) + ")");
        }
    }
}

void redirect_log(Logger& log)
{
    log.file.close();
    log.file.clear();
    DeleteFileW(L"X4_READONLY_CAPABILITY_REPORT.txt");
    log.file.open("X4_MIXER_DRILLDOWN_REPORT.txt",
        std::ios::binary | std::ios::trunc);
}
}

int wmain(int argc, wchar_t* argv[])
{
    Logger log;
    redirect_log(log);
    log.line("Sound Blaster X4 ARM64 read-only mixer drill-down probe");
    log.line("READ-ONLY: session validation + Malcolm support + AudioControl information/range/level/mute GET only.");
    log.line("No mixer SET, no Malcolm SET(0x12), no Direct Mode setter, no raw-command CLI.");

    if (argc > 2)
    {
        log.line("Usage: x4-mixer-readonly-drilldown.exe [COMx]");
        return 2;
    }

    std::wstring const port = argc == 2 ? argv[1] : find_x4_control_port();
    if (port.empty())
    {
        log.line("X4 MI_01 serial interface not found.");
        return 10;
    }
    log.line("Port: " + narrow_ascii(port));

    HANDLE const handle = CreateFileW(normalize_port(port).c_str(),
        GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        log.line("CreateFileW failed: " + std::to_string(GetLastError()));
        log.line("If Creative App is running, fully close it before retrying this independent CTCDC probe.");
        return 20;
    }

    auto const finish = [&](int code)
    {
        EscapeCommFunction(handle, CLRDTR);
        CloseHandle(handle);
        log.line("Done. Upload X4_MIXER_DRILLDOWN_REPORT.txt.");
        return code;
    };

    if (!configure_like_ctcdc(handle, log))
        return finish(21);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    log.line();
    log.line("=== Session gate ===");
    {
        Query const q{"Session", "GetMaximumPayloadSize", kCmdMaxPayload, {}};
        auto const frames = transact_capture(handle, q, log, 3000, 120);
        auto const* frame = find_command(frames, kCmdMaxPayload);
        if (!frame || frame->payload.size() != 2)
        {
            log.line("STOP: Maximum Payload Size did not validate. No mixer drill-down queries sent.");
            log.line("If Creative App is running, fully close it before retrying.");
            return finish(31);
        }
        log.line("VALID Maximum Payload Size: " +
            std::to_string(u16le(frame->payload, 0)));
    }

    {
        Query const q{"Session", "GetFirmwareVersionString", kCmdFirmware, {0x02}};
        auto const frames = transact_capture(handle, q, log, 3000, 120);
        auto const* frame = find_command(frames, kCmdFirmware);
        if (!frame || frame->payload.empty() || frame->payload[0] != 0x02)
        {
            log.line("STOP: firmware response did not validate. No mixer drill-down queries sent.");
            return finish(32);
        }
        log.line("VALID firmware response framing.");
    }

    {
        Query const q{"Session", "QueryButtonsAvailable", kCmdButtons, {0x05}};
        auto const frames = transact_capture(handle, q, log, 3000, 120);
        auto const* frame = find_command(frames, kCmdButtons);
        if (!frame || frame->payload.empty() || frame->payload[0] != 0x05)
        {
            log.line("STOP: buttons response did not validate. No mixer drill-down queries sent.");
            return finish(33);
        }
        log.line("VALID QueryButtonsAvailable response framing.");
    }

    log.line();
    log.line("=== Malcolm sub-feature support (read-only) ===");
    {
        Query const q{"Support", "GetMalcolmSubFeatureSupport", 0x10, {}};
        auto const frames = transact_capture(handle, q, log, 1500, 80);
        if (auto const* frame = find_command(frames, 0x10);
            frame && frame->payload.size() >= 8)
        {
            log.line("FeatureMask: " + hex_u32(u32le(frame->payload, 0)));
            log.line("UnavailableMask: " + hex_u32(u32le(frame->payload, 4)));
        }
        else
        {
            log_ack_if_present(log, frames, 0x10);
            log.line("No parseable command-0x10 support payload; continuing with mixer GET-only drill-down.");
        }
    }

    log.line();
    log.line("=== AudioControl information ===");
    std::vector<MixerDescriptor> descriptors;
    {
        Query const q{"Mixer", "GetAudioControlInformation", kCmdAudioControlInfo, {}};
        auto const frames = transact_capture(handle, q, log, 2000, 100);
        auto const* frame = find_command(frames, kCmdAudioControlInfo);
        if (!frame || frame->payload.size() < 4)
        {
            log_ack_if_present(log, frames, kCmdAudioControlInfo);
            log.line("STOP: no parseable AudioControlInformation response.");
            return finish(40);
        }

        auto const total = frame->payload[0];
        auto const count = frame->payload[1];
        auto const additionalPacket = frame->payload[2];
        auto const baseIndex = frame->payload[3];
        log.line("Total=" + std::to_string(total) +
            " Count=" + std::to_string(count) +
            " AdditionalPacket=" + std::to_string(additionalPacket) +
            " AudioControlIndex=" + std::to_string(baseIndex));

        if (count > 32 ||
            frame->payload.size() < static_cast<std::size_t>(4 + count * 2))
        {
            log.line("STOP: AudioControlInformation descriptor count/length is not safe to parse.");
            return finish(41);
        }

        for (std::uint8_t i = 0; i < count; ++i)
        {
            auto const raw = u16le(frame->payload,
                4 + static_cast<std::size_t>(i) * 2);
            MixerDescriptor d{};
            d.index = static_cast<std::uint8_t>(baseIndex + i);
            d.raw = raw;
            d.type = static_cast<std::uint8_t>(raw & 0x003F);
            d.hasVolume = (raw & 0x0040) != 0;
            d.hasMute = (raw & 0x0080) != 0;
            d.isSource = (raw & 0x0100) != 0;
            d.isRecording = (raw & 0x0200) != 0;
            d.hasCustomName = (raw & 0x0400) != 0;
            d.isDbLevel = (raw & 0x0800) != 0;
            d.channelNibble = static_cast<std::uint8_t>((raw >> 12) & 0x0F);
            descriptors.push_back(d);

            log.line("index=" + std::to_string(d.index) +
                " raw=" + hex_u16(raw) +
                " type=" + audio_control_type_name(d.type) +
                " volume=" + (d.hasVolume ? "Y" : "N") +
                " mute=" + (d.hasMute ? "Y" : "N") +
                " source=" + (d.isSource ? "Y" : "N") +
                " recording=" + (d.isRecording ? "Y" : "N") +
                " customName=" + (d.hasCustomName ? "Y" : "N") +
                " dB=" + (d.isDbLevel ? "Y" : "N") +
                " channelNibble=" + std::to_string(d.channelNibble));
        }

        auto const used = static_cast<std::size_t>(4 + count * 2);
        if (frame->payload.size() > used)
        {
            std::vector<std::uint8_t> trailing(
                frame->payload.begin() + used, frame->payload.end());
            log.line("Trailing payload bytes after Count descriptors (not interpreted): " +
                hex_bytes(trailing));
        }
    }

    if (descriptors.empty())
    {
        log.line("STOP: no AudioControl descriptors were reported.");
        return finish(42);
    }

    log.line();
    log.line("=== AudioControl level ranges (read-only) ===");
    {
        std::vector<std::uint8_t> payload(33, 0);
        payload[0] = static_cast<std::uint8_t>(descriptors.size());
        for (std::size_t i = 0; i < descriptors.size() && i < 32; ++i)
            payload[1 + i] = descriptors[i].index;

        Query const q{"Mixer", "GetAudioLevelRanges", 0x22, payload};
        auto const frames = transact_capture(handle, q, log, 2500, 120);
        if (auto const* frame = find_command(frames, 0x22);
            frame && frame->payload.size() >= 2)
        {
            auto const count = frame->payload[0];
            auto const additionalPacket = frame->payload[1];
            log.line("RangeCount=" + std::to_string(count) +
                " AdditionalPacket=" + std::to_string(additionalPacket));

            auto const needed = static_cast<std::size_t>(2 + count * 7);
            if (frame->payload.size() >= needed)
            {
                for (std::uint8_t i = 0; i < count; ++i)
                {
                    auto const off = static_cast<std::size_t>(2 + i * 7);
                    auto const index = frame->payload[off];
                    auto const maxValue = u16le(frame->payload, off + 1);
                    auto const minValue = u16le(frame->payload, off + 3);
                    auto const stepValue = u16le(frame->payload, off + 5);
                    log.line("range index=" + std::to_string(index) +
                        " maxRaw=" + hex_u16(maxValue) +
                        " maxSigned=" + std::to_string(static_cast<std::int16_t>(maxValue)) +
                        " minRaw=" + hex_u16(minValue) +
                        " minSigned=" + std::to_string(static_cast<std::int16_t>(minValue)) +
                        " stepRaw=" + hex_u16(stepValue) +
                        " stepSigned=" + std::to_string(static_cast<std::int16_t>(stepValue)));
                }
            }
            else
            {
                log.line("Range payload shorter than Count implies; raw bytes preserved above.");
            }
        }
        else
        {
            log_ack_if_present(log, frames, 0x22);
            log.line("No parseable command-0x22 range payload.");
        }
    }

    log.line();
    log.line("=== Current AudioControl levels (GET only) ===");
    for (auto const& d : descriptors)
    {
        if (!d.hasVolume)
            continue;

        Query const q{"Mixer", "AudioLevel GET index=" + std::to_string(d.index),
            0x23, {0x01, d.index}};
        auto const frames = transact_capture(handle, q, log, 1500, 80);
        if (auto const* frame = find_command(frames, 0x23);
            frame && frame->payload.size() >= 3)
        {
            auto const current = u16le(frame->payload, 1);
            log.line("level index=" + std::to_string(frame->payload[0]) +
                " raw=" + hex_u16(current) +
                " signed=" + std::to_string(static_cast<std::int16_t>(current)));
        }
        else
        {
            log_ack_if_present(log, frames, 0x23);
            log.line("No parseable AudioLevel GET payload for index " +
                std::to_string(d.index));
        }
    }

    log.line();
    log.line("=== Current AudioControl mute states (GET only) ===");
    for (auto const& d : descriptors)
    {
        if (!d.hasMute)
            continue;

        Query const q{"Mixer", "AudioMute GET index=" + std::to_string(d.index),
            0x24, {0x01, d.index}};
        auto const frames = transact_capture(handle, q, log, 1500, 80);
        if (auto const* frame = find_command(frames, 0x24);
            frame && frame->payload.size() >= 2)
        {
            log.line("mute index=" + std::to_string(frame->payload[0]) +
                " value=" + std::to_string(frame->payload[1]));
        }
        else
        {
            log_ack_if_present(log, frames, 0x24);
            log.line("No parseable AudioMute GET payload for index " +
                std::to_string(d.index));
        }
    }

    log.line();
    log.line("Read-only mixer drill-down complete. No state-changing command was sent.");
    return finish(0);
}
