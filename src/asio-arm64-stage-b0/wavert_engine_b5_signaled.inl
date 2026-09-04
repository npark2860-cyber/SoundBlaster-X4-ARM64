// B5 high-rate notification processor.
//
// Included after wavert_engine_b5.cpp by both architecture adapters. This is
// intentionally separate from the proven wait-based path: the dual-event mux
// waits on Render/Capture together, then calls this method only for an event it
// has already observed as signaled.

X4WaveRtB5ProcessResult X4WaveRtEngineB5::process_signaled_notification(
    ULONG* packet_number_out,
    BOOL* more_data_out,
    X4WaveRtB5NotificationObserver observer,
    void* observer_context,
    bool trace_notification) {

    if (packet_number_out) *packet_number_out = 0;
    if (more_data_out) *more_data_out = FALSE;

    if (!entered_run_ || !event_ || pin_ == INVALID_HANDLE_VALUE) {
        strcpy_s(last_message_, "B5 signaled process FAILED: RUN/event state invalid");
        return X4WaveRtB5ProcessResult::Failed;
    }

    auto make_get_property = [](const GUID& set, ULONG id) {
        KSPROPERTY property{};
        property.Set = set;
        property.Id = id;
        property.Flags = KSPROPERTY_TYPE_GET;
        return property;
    };

    ULONG packet_number = 0;
    UINT64 position_blocks = 0;
    UINT64 qpc = 0;
    BOOL more_data = FALSE;
    DWORD returned = 0;

    if (config_.direction == X4WaveRtB5Direction::Render) {
        KSPROPERTY packet_property = make_get_property(
            KSPROPSETID_RtAudio, KSPROPERTY_RTAUDIO_PACKETCOUNT);
        if (!DeviceIoControl(
                pin_, IOCTL_KS_PROPERTY,
                &packet_property, sizeof(packet_property),
                &packet_number, sizeof(packet_number),
                &returned, nullptr)) {
            sprintf_s(last_message_, sizeof(last_message_),
                      "B5 RENDER PACKETCOUNT FAILED Win32=%lu", GetLastError());
            return X4WaveRtB5ProcessResult::Failed;
        }

        KSAUDIO_PRESENTATION_POSITION position{};
        KSPROPERTY position_property = make_get_property(
            KSPROPSETID_RtAudio, KSPROPERTY_RTAUDIO_PRESENTATION_POSITION);
        if (!DeviceIoControl(
                pin_, IOCTL_KS_PROPERTY,
                &position_property, sizeof(position_property),
                &position, sizeof(position),
                &returned, nullptr)) {
            sprintf_s(last_message_, sizeof(last_message_),
                      "B5 RENDER PRESENTATION_POSITION FAILED Win32=%lu", GetLastError());
            return X4WaveRtB5ProcessResult::Failed;
        }

        position_blocks = position.u64PositionInBlocks;
        qpc = position.u64QPCPosition;
        if (have_previous_position_ && position_blocks < previous_position_) {
            ++stats_.position_regressions;
        }
        previous_position_ = position_blocks;
        have_previous_position_ = true;
    } else {
        KSRTAUDIO_GETREADPACKET_INFO info{};
        KSPROPERTY read_property = make_get_property(
            KSPROPSETID_RtAudio, KSPROPERTY_RTAUDIO_GETREADPACKET);
        if (!DeviceIoControl(
                pin_, IOCTL_KS_PROPERTY,
                &read_property, sizeof(read_property),
                &info, sizeof(info),
                &returned, nullptr)) {
            const DWORD error = GetLastError();
            if (error == ERROR_NOT_READY) {
                strcpy_s(last_message_, "B5 CAPTURE GETREADPACKET NOT_READY");
                return X4WaveRtB5ProcessResult::NoData;
            }
            sprintf_s(last_message_, sizeof(last_message_),
                      "B5 CAPTURE GETREADPACKET FAILED Win32=%lu", error);
            return X4WaveRtB5ProcessResult::Failed;
        }

        packet_number = info.PacketNumber;
        qpc = info.PerformanceCount;
        more_data = info.MoreData;
    }

    if (have_previous_packet_ && packet_number != previous_packet_ + 1) {
        ++stats_.packet_discontinuities;
    }
    previous_packet_ = packet_number;
    have_previous_packet_ = true;

    const ULONG notification_index = stats_.notifications;
    ++stats_.notifications;
    stats_.last_packet = packet_number;

    if (packet_number_out) *packet_number_out = packet_number;
    if (more_data_out) *more_data_out = more_data;

    if (trace_notification) {
        const char* dir =
            config_.direction == X4WaveRtB5Direction::Render ? "RENDER" : "CAPTURE";
        std::printf(
            "B5 %s notification=%lu packet=%lu slot=%lu position=%llu qpc=%llu moreData=%d thread=%lu\n",
            dir, stats_.notifications, packet_number,
            packet_number % config_.notification_count,
            static_cast<unsigned long long>(position_blocks),
            static_cast<unsigned long long>(qpc),
            more_data ? 1 : 0, GetCurrentThreadId());
    }

    if (observer && !observer(observer_context, notification_index, packet_number)) {
        return X4WaveRtB5ProcessResult::Failed;
    }
    return X4WaveRtB5ProcessResult::Notification;
}
