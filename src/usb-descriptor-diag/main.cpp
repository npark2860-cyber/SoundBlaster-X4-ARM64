#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <initguid.h>
#include <usbiodef.h>
#include <usbioctl.h>
#include <usb.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")

namespace
{
constexpr USHORT kCreativeVid = 0x041E;
constexpr USHORT kX4Pid = 0x3278;

std::string utf8(std::wstring const& value)
{
    if (value.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::string hex_byte(unsigned v)
{
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (v & 0xFFu);
    return ss.str();
}

std::string hex_word(unsigned v)
{
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << (v & 0xFFFFu);
    return ss.str();
}

std::string raw_hex(const unsigned char* p, size_t n)
{
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i)
    {
        if (i) ss << ' ';
        ss << std::setw(2) << static_cast<unsigned>(p[i]);
    }
    return ss.str();
}

USHORT le16(const unsigned char* p)
{
    return static_cast<USHORT>(p[0] | (static_cast<USHORT>(p[1]) << 8));
}

const char* connection_status_name(USB_CONNECTION_STATUS s)
{
    switch (s)
    {
    case NoDeviceConnected: return "NoDeviceConnected";
    case DeviceConnected: return "DeviceConnected";
    case DeviceFailedEnumeration: return "DeviceFailedEnumeration";
    case DeviceGeneralFailure: return "DeviceGeneralFailure";
    case DeviceCausedOvercurrent: return "DeviceCausedOvercurrent";
    case DeviceNotEnoughPower: return "DeviceNotEnoughPower";
    case DeviceNotEnoughBandwidth: return "DeviceNotEnoughBandwidth";
    case DeviceHubNestedTooDeeply: return "DeviceHubNestedTooDeeply";
    case DeviceInLegacyHub: return "DeviceInLegacyHub";
    case DeviceEnumerating: return "DeviceEnumerating";
    case DeviceReset: return "DeviceReset";
    default: return "Unknown";
    }
}

const char* descriptor_type_name(unsigned type)
{
    switch (type)
    {
    case 0x01: return "DEVICE";
    case 0x02: return "CONFIGURATION";
    case 0x03: return "STRING";
    case 0x04: return "INTERFACE";
    case 0x05: return "ENDPOINT";
    case 0x06: return "DEVICE_QUALIFIER";
    case 0x07: return "OTHER_SPEED_CONFIGURATION";
    case 0x08: return "INTERFACE_POWER";
    case 0x0B: return "IAD";
    case 0x24: return "CS_INTERFACE";
    case 0x25: return "CS_ENDPOINT";
    default: return "OTHER";
    }
}

const char* uac2_ac_subtype_name(unsigned subtype)
{
    switch (subtype)
    {
    case 0x01: return "HEADER";
    case 0x02: return "INPUT_TERMINAL";
    case 0x03: return "OUTPUT_TERMINAL";
    case 0x04: return "MIXER_UNIT";
    case 0x05: return "SELECTOR_UNIT";
    case 0x06: return "FEATURE_UNIT";
    case 0x07: return "EFFECT_UNIT";
    case 0x08: return "PROCESSING_UNIT";
    case 0x09: return "EXTENSION_UNIT";
    case 0x0A: return "CLOCK_SOURCE";
    case 0x0B: return "CLOCK_SELECTOR";
    case 0x0C: return "CLOCK_MULTIPLIER";
    case 0x0D: return "SAMPLE_RATE_CONVERTER";
    default: return "UNKNOWN_AC_SUBTYPE";
    }
}

void parse_configuration(std::ofstream& out, const unsigned char* data, size_t size)
{
    unsigned currentIf = 0xFFFFFFFFu;
    unsigned currentAlt = 0;
    unsigned currentClass = 0;
    unsigned currentSubClass = 0;
    unsigned currentProtocol = 0;
    unsigned extensionUnits = 0;
    unsigned vendorInterfaces = 0;
    unsigned csInterfaceCount = 0;

    out << "\n=== RAW CONFIGURATION DESCRIPTOR ===\n";
    out << "Bytes available: " << size << "\n";

    size_t off = 0;
    while (off + 2 <= size)
    {
        unsigned len = data[off];
        unsigned type = data[off + 1];
        if (len < 2 || off + len > size)
        {
            out << "Offset 0x" << std::hex << std::uppercase << off << std::dec
                << ": malformed descriptor length=" << len << "\n";
            break;
        }

        out << "\nOffset 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << off
            << std::dec << std::setfill(' ') << "  Length=" << len
            << " Type=0x" << hex_byte(type) << " (" << descriptor_type_name(type) << ")\n";
        out << "  Raw: " << raw_hex(data + off, len) << "\n";

        if (type == 0x02 && len >= 9)
        {
            out << "  wTotalLength=" << le16(data + off + 2)
                << " bNumInterfaces=" << static_cast<unsigned>(data[off + 4])
                << " bConfigurationValue=" << static_cast<unsigned>(data[off + 5])
                << " bmAttributes=0x" << hex_byte(data[off + 7])
                << " bMaxPower=" << static_cast<unsigned>(data[off + 8]) << "\n";
        }
        else if (type == 0x04 && len >= 9)
        {
            currentIf = data[off + 2];
            currentAlt = data[off + 3];
            currentClass = data[off + 5];
            currentSubClass = data[off + 6];
            currentProtocol = data[off + 7];
            if (currentClass == 0xFF)
                ++vendorInterfaces;

            out << "  Interface=" << currentIf
                << " Alt=" << currentAlt
                << " Endpoints=" << static_cast<unsigned>(data[off + 4])
                << " Class=0x" << hex_byte(currentClass)
                << " SubClass=0x" << hex_byte(currentSubClass)
                << " Protocol=0x" << hex_byte(currentProtocol);
            if (currentClass == 0x01 && currentSubClass == 0x01)
                out << "  [AudioControl]";
            else if (currentClass == 0x01 && currentSubClass == 0x02)
                out << "  [AudioStreaming]";
            else if (currentClass == 0xFF)
                out << "  [VENDOR CLASS]";
            out << "\n";
        }
        else if (type == 0x05 && len >= 7)
        {
            out << "  EndpointAddress=0x" << hex_byte(data[off + 2])
                << " Attributes=0x" << hex_byte(data[off + 3])
                << " MaxPacketSize=" << le16(data + off + 4)
                << " Interval=" << static_cast<unsigned>(data[off + 6]) << "\n";
        }
        else if (type == 0x24 && len >= 3)
        {
            ++csInterfaceCount;
            unsigned subtype = data[off + 2];
            out << "  CS_INTERFACE subtype=0x" << hex_byte(subtype)
                << " on Interface=" << currentIf << " Alt=" << currentAlt;

            if (currentClass == 0x01 && currentSubClass == 0x01)
            {
                out << "  UAC AudioControl " << uac2_ac_subtype_name(subtype);
                if (subtype == 0x09)
                {
                    ++extensionUnits;
                    out << "  <<< UAC2 EXTENSION UNIT CANDIDATE >>>";
                }
                else if (subtype == 0x08 && currentProtocol != 0x20)
                {
                    ++extensionUnits;
                    out << "  <<< UAC1 EXTENSION UNIT CANDIDATE >>>";
                }
            }
            out << "\n";
        }

        off += len;
    }

    out << "\n=== SUMMARY ===\n";
    out << "Class-specific interface descriptors: " << csInterfaceCount << "\n";
    out << "Audio extension-unit candidates: " << extensionUnits << "\n";
    out << "Vendor-class (0xFF) interface descriptors: " << vendorInterfaces << "\n";
}

bool fetch_config_descriptor(HANDLE hub, ULONG port, std::ofstream& out)
{
    constexpr ULONG kMaxDescriptor = 16384;
    std::vector<unsigned char> buffer(sizeof(USB_DESCRIPTOR_REQUEST) + kMaxDescriptor, 0);
    auto* req = reinterpret_cast<PUSB_DESCRIPTOR_REQUEST>(buffer.data());
    req->ConnectionIndex = port;
    req->SetupPacket.bmRequest = 0x80;
    req->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    req->SetupPacket.wValue = static_cast<USHORT>(USB_CONFIGURATION_DESCRIPTOR_TYPE << 8);
    req->SetupPacket.wIndex = 0;
    req->SetupPacket.wLength = static_cast<USHORT>(kMaxDescriptor);

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(hub,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              buffer.data(),
                              sizeof(USB_DESCRIPTOR_REQUEST),
                              buffer.data(),
                              static_cast<DWORD>(buffer.size()),
                              &bytesReturned,
                              nullptr);
    if (!ok)
    {
        out << "IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION failed. Win32Error=" << GetLastError() << "\n";
        return false;
    }

    if (bytesReturned <= sizeof(USB_DESCRIPTOR_REQUEST))
    {
        out << "Descriptor request returned no descriptor payload. bytesReturned=" << bytesReturned << "\n";
        return false;
    }

    const unsigned char* desc = buffer.data() + sizeof(USB_DESCRIPTOR_REQUEST);
    size_t available = bytesReturned - sizeof(USB_DESCRIPTOR_REQUEST);
    if (available < 9 || desc[1] != USB_CONFIGURATION_DESCRIPTOR_TYPE)
    {
        out << "Unexpected configuration descriptor payload. available=" << available << "\n";
        out << "Raw: " << raw_hex(desc, available) << "\n";
        return false;
    }

    USHORT total = le16(desc + 2);
    size_t parseSize = std::min<size_t>(total, available);
    out << "Configuration wTotalLength=" << total << " returnedPayload=" << available << " parseBytes=" << parseSize << "\n";
    parse_configuration(out, desc, parseSize);
    return true;
}

bool scan_hub(std::wstring const& hubPath, std::ofstream& out, unsigned& matches)
{
    HANDLE hub = CreateFileW(hubPath.c_str(), GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, 0, nullptr);
    if (hub == INVALID_HANDLE_VALUE)
    {
        out << "Unable to open hub: " << utf8(hubPath) << " error=" << GetLastError() << "\n";
        return false;
    }

    USB_NODE_INFORMATION node{};
    node.NodeType = UsbHub;
    DWORD bytes = 0;
    if (!DeviceIoControl(hub, IOCTL_USB_GET_NODE_INFORMATION,
                         &node, sizeof(node), &node, sizeof(node), &bytes, nullptr))
    {
        CloseHandle(hub);
        return false;
    }

    UCHAR ports = node.u.HubInformation.HubDescriptor.bNumberOfPorts;
    for (ULONG port = 1; port <= ports; ++port)
    {
        std::vector<unsigned char> connBuffer(sizeof(USB_NODE_CONNECTION_INFORMATION_EX) + 4096, 0);
        auto* conn = reinterpret_cast<PUSB_NODE_CONNECTION_INFORMATION_EX>(connBuffer.data());
        conn->ConnectionIndex = port;
        DWORD returned = 0;
        if (!DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                             connBuffer.data(), static_cast<DWORD>(connBuffer.size()),
                             connBuffer.data(), static_cast<DWORD>(connBuffer.size()),
                             &returned, nullptr))
            continue;

        if (conn->DeviceDescriptor.idVendor != kCreativeVid || conn->DeviceDescriptor.idProduct != kX4Pid)
            continue;

        ++matches;
        out << "============================================================\n";
        out << "Sound Blaster X4 USB device found\n";
        out << "HubPath: " << utf8(hubPath) << "\n";
        out << "Port: " << port << "\n";
        out << "ConnectionStatus: " << connection_status_name(conn->ConnectionStatus)
            << " (" << static_cast<unsigned>(conn->ConnectionStatus) << ")\n";
        out << "VID: 0x" << hex_word(conn->DeviceDescriptor.idVendor)
            << " PID: 0x" << hex_word(conn->DeviceDescriptor.idProduct)
            << " bcdDevice: 0x" << hex_word(conn->DeviceDescriptor.bcdDevice) << "\n";
        out << "DeviceClass: 0x" << hex_byte(conn->DeviceDescriptor.bDeviceClass)
            << " DeviceSubClass: 0x" << hex_byte(conn->DeviceDescriptor.bDeviceSubClass)
            << " DeviceProtocol: 0x" << hex_byte(conn->DeviceDescriptor.bDeviceProtocol) << "\n";
        out << "CurrentConfigurationValue: " << static_cast<unsigned>(conn->CurrentConfigurationValue) << "\n";
        out << "DeviceAddress: " << conn->DeviceAddress << "\n";
        out << "NumberOfOpenPipes: " << conn->NumberOfOpenPipes << "\n";

        fetch_config_descriptor(hub, port, out);
        out << "\n";
    }

    CloseHandle(hub);
    return true;
}
}

int wmain()
{
    std::ofstream out("x4-usb-descriptor.txt", std::ios::binary | std::ios::trunc);
    if (!out)
    {
        std::cerr << "Cannot create x4-usb-descriptor.txt\n";
        return 2;
    }

    out << "Sound Blaster X4 raw USB configuration descriptor diagnostic\n";
    out << "Target: VID_041E PID_3278\n";
    out << "Read-only: no vendor/audio/HID/serial command writes are performed.\n\n";

    GUID guid = GUID_DEVINTERFACE_USB_HUB;
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE)
    {
        out << "SetupDiGetClassDevsW failed: " << GetLastError() << "\n";
        return 3;
    }

    unsigned hubs = 0;
    unsigned matches = 0;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA ifData{};
        ifData.cbSize = sizeof(ifData);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index, &ifData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifData, nullptr, 0, &required, nullptr);
        if (!required)
            continue;
        std::vector<unsigned char> detailBuffer(required, 0);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &ifData, detail, required, nullptr, nullptr))
            continue;

        ++hubs;
        scan_hub(detail->DevicePath, out, matches);
    }

    SetupDiDestroyDeviceInfoList(set);

    out << "============================================================\n";
    out << "USB hub interfaces scanned: " << hubs << "\n";
    out << "Matching X4 devices found: " << matches << "\n";
    out.close();

    std::cout << "USB hub interfaces scanned: " << hubs << "\n";
    std::cout << "Matching X4 devices found: " << matches << "\n";
    std::cout << "Output: x4-usb-descriptor.txt\n";
    if (!matches)
    {
        std::cout << "No X4 was found through the USB hub connection table.\n";
        return 4;
    }
    return 0;
}
