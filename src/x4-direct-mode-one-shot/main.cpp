#define wmain x4_ctcdc_legacy_probe_wmain
#include "../serial-poc/session-open-probe.cpp"
#undef wmain

namespace
{
constexpr std::array<std::uint8_t, 6> kDirectModeOff{
    0x5A, 0x39, 0x03, 0x00, 0x05, 0x00};
constexpr std::array<std::uint8_t, 6> kDirectModeOn{
    0x5A, 0x39, 0x03, 0x00, 0x05, 0x01};
}

int wmain(int argc, wchar_t* argv[])
{
    Logger log;
    log.line("Sound Blaster X4 CTCDC Direct Mode one-shot probe");
    log.line("Sequence: validated CTCDC fast-path session, then exactly one Direct Mode state-changing frame.");
    log.line("No unlock response or SW_MODE1 is sent.");

    if (argc < 2 || argc > 3)
    {
        log.line("Usage: x4-direct-mode-one-shot.exe on|off [COMx]");
        return 2;
    }

    auto mode = upper(argv[1]);
    bool turnOn = false;
    if (mode == L"ON")
        turnOn = true;
    else if (mode != L"OFF")
    {
        log.line("Usage: x4-direct-mode-one-shot.exe on|off [COMx]");
        return 2;
    }

    std::wstring port = argc == 3 ? argv[2] : find_x4_control_port();
    if (port.empty())
    {
        log.line("Sound Blaster X4 MI_01 serial interface was not found.");
        return 10;
    }

    log.line("Requested state: " + std::string(turnOn ? "ON" : "OFF"));
    log.line("Port: " + narrow_ascii(port));

    auto const path = normalize_port(port);
    HANDLE handle = CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        log.line("CreateFileW failed: " + std::to_string(GetLastError()));
        return 20;
    }

    int result = 0;

    if (!configure_like_ctcdc(handle, log))
    {
        result = 21;
        goto done;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!write_exact(handle, kMaxPayloadQuery, log, "GetMaximumPayloadSize"))
    {
        result = 30;
        goto done;
    }
    {
        auto rx = collect_available(handle, 3000, 120, log);
        log_rx(log, "GetMaximumPayloadSize", rx);
        auto frame = first_frame(rx);
        if (!frame.valid || frame.command != 0x03 || frame.payload.size() != 2)
        {
            log.line("STOP: no valid command-0x03 maximum-payload response. Direct Mode command NOT sent.");
            result = 31;
            goto done;
        }
        auto const maxPayload = static_cast<std::uint16_t>(frame.payload[0] | (frame.payload[1] << 8));
        log.line("VALID Maximum Payload Size: " + std::to_string(maxPayload) + " bytes");
    }

    if (!write_exact(handle, kFirmwareVersionQuery, log, "GetFirmwareVersionString"))
    {
        result = 40;
        goto done;
    }
    {
        auto rx = collect_available(handle, 3000, 120, log);
        log_rx(log, "GetFirmwareVersionString", rx);
        auto frame = first_frame(rx);
        if (!frame.valid || frame.command != 0x09 || frame.payload.empty() || frame.payload[0] != 0x02)
        {
            log.line("STOP: firmware-version response did not match command 0x09 selector 0x02. Direct Mode command NOT sent.");
            result = 41;
            goto done;
        }
        log.line("VALID GetFirmwareVersionString response framing.");
    }

    if (!write_exact(handle, kButtonsAvailableQuery, log, "QueryButtonsAvailable"))
    {
        result = 50;
        goto done;
    }
    {
        auto rx = collect_available(handle, 3000, 120, log);
        log_rx(log, "QueryButtonsAvailable", rx);
        auto frame = first_frame(rx);
        if (!frame.valid || frame.command != 0x26 || frame.payload.empty() || frame.payload[0] != 0x05)
        {
            log.line("STOP: buttons response did not match command 0x26 selector 0x05. Direct Mode command NOT sent.");
            result = 51;
            goto done;
        }
        log.line("VALID QueryButtonsAvailable response framing.");
    }

    if (turnOn)
    {
        if (!write_exact(handle, kDirectModeOn, log, "DirectMode ON"))
        {
            result = 60;
            goto done;
        }
        log.line("SENT Direct Mode ON: 5A 39 03 00 05 01");
    }
    else
    {
        if (!write_exact(handle, kDirectModeOff, log, "DirectMode OFF"))
        {
            result = 61;
            goto done;
        }
        log.line("SENT Direct Mode OFF: 5A 39 03 00 05 00");
    }

    log.line("One-shot command sent. Verify the physical X4 state; no Direct Mode response bytes are assumed.");

done:
    EscapeCommFunction(handle, CLRDTR);
    CloseHandle(handle);
    log.line("Done. Log file: x4-ctcdc-probe.txt");
    return result;
}
