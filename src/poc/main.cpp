#include <algorithm>
#include <array>
#include <cstdint>
#include <cwctype>
#include <iomanip>
#include <iostream>
#include <string>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>

#pragma comment(lib, "windowsapp")

namespace
{
using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Storage::Streams;

constexpr wchar_t kDeviceName[] = L"Control for SB1815";

const guid kServiceUuid{
    0xb7860001, 0x11b8, 0xb681,
    {0x63, 0x43, 0x5a, 0x6c, 0x22, 0x86, 0x63, 0x3f}};

const guid kWriteUuid{
    0xb7860002, 0x11b8, 0xb681,
    {0x63, 0x43, 0x5a, 0x6c, 0x22, 0x86, 0x63, 0x3f}};

constexpr std::array<uint8_t, 6> kDirectModeOff{
    0x5A, 0x39, 0x03, 0x00, 0x05, 0x00};

constexpr std::array<uint8_t, 6> kDirectModeOn{
    0x5A, 0x39, 0x03, 0x00, 0x05, 0x01};

bool has_property(GattCharacteristicProperties value, GattCharacteristicProperties flag)
{
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

const wchar_t* status_name(GattCommunicationStatus status)
{
    switch (status)
    {
    case GattCommunicationStatus::Success:
        return L"Success";
    case GattCommunicationStatus::Unreachable:
        return L"Unreachable";
    case GattCommunicationStatus::ProtocolError:
        return L"ProtocolError";
    case GattCommunicationStatus::AccessDenied:
        return L"AccessDenied";
    default:
        return L"Unknown";
    }
}

IBuffer make_buffer(std::array<uint8_t, 6> const& bytes)
{
    DataWriter writer;
    writer.WriteBytes(winrt::array_view<uint8_t const>{bytes.data(), bytes.data() + bytes.size()});
    return writer.DetachBuffer();
}

void print_command(std::array<uint8_t, 6> const& bytes)
{
    std::wcout << L"Command: ";
    for (auto byte : bytes)
    {
        std::wcout << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
                   << static_cast<unsigned>(byte);
    }
    std::wcout << std::dec << L"\n";
}

int run(bool turnOn)
{
    auto selector = BluetoothLEDevice::GetDeviceSelectorFromDeviceName(hstring{kDeviceName});
    auto deviceInfos = DeviceInformation::FindAllAsync(selector).get();

    if (deviceInfos.Size() == 0)
    {
        std::wcerr << L"X4 BLE control device not found: " << kDeviceName << L"\n"
                   << L"Make sure Bluetooth is enabled and the X4 control device is visible/paired in Windows.\n";
        return 10;
    }

    BluetoothLEDevice device{nullptr};

    for (auto const& info : deviceInfos)
    {
        auto candidate = BluetoothLEDevice::FromIdAsync(info.Id()).get();
        if (candidate != nullptr && candidate.Name() == hstring{kDeviceName})
        {
            device = candidate;
            break;
        }
    }

    if (device == nullptr)
    {
        std::wcerr << L"Windows found the X4 BLE entry but could not open it.\n"
                   << L"Pair/allow the device in Windows Bluetooth settings, then retry.\n";
        return 11;
    }

    std::wcout << L"Device: " << device.Name().c_str() << L"\n";

    auto serviceResult = device.GetGattServicesForUuidAsync(kServiceUuid, BluetoothCacheMode::Uncached).get();
    if (serviceResult.Status() != GattCommunicationStatus::Success)
    {
        std::wcerr << L"GATT service lookup failed: " << status_name(serviceResult.Status()) << L"\n";
        return 20;
    }

    auto services = serviceResult.Services();
    if (services.Size() == 0)
    {
        std::wcerr << L"Confirmed X4 service UUID was not exposed by Windows.\n";
        return 21;
    }

    auto service = services.GetAt(0);
    auto characteristicResult = service.GetCharacteristicsForUuidAsync(kWriteUuid, BluetoothCacheMode::Uncached).get();

    if (characteristicResult.Status() != GattCommunicationStatus::Success)
    {
        std::wcerr << L"Write characteristic lookup failed: " << status_name(characteristicResult.Status()) << L"\n";
        return 30;
    }

    auto characteristics = characteristicResult.Characteristics();
    if (characteristics.Size() == 0)
    {
        std::wcerr << L"Confirmed X4 write characteristic UUID was not exposed by Windows.\n";
        return 31;
    }

    auto characteristic = characteristics.GetAt(0);
    auto properties = characteristic.CharacteristicProperties();
    auto const& command = turnOn ? kDirectModeOn : kDirectModeOff;

    print_command(command);

    GattCommunicationStatus lastStatus = GattCommunicationStatus::Unreachable;

    if (has_property(properties, GattCharacteristicProperties::Write))
    {
        lastStatus = characteristic
                         .WriteValueAsync(make_buffer(command), GattWriteOption::WriteWithResponse)
                         .get();

        std::wcout << L"WriteWithResponse: " << status_name(lastStatus) << L"\n";
        if (lastStatus == GattCommunicationStatus::Success)
        {
            std::wcout << L"Direct Mode " << (turnOn ? L"ON" : L"OFF") << L" command sent.\n";
            return 0;
        }
    }

    if (has_property(properties, GattCharacteristicProperties::WriteWithoutResponse))
    {
        lastStatus = characteristic
                         .WriteValueAsync(make_buffer(command), GattWriteOption::WriteWithoutResponse)
                         .get();

        std::wcout << L"WriteWithoutResponse: " << status_name(lastStatus) << L"\n";
        if (lastStatus == GattCommunicationStatus::Success)
        {
            std::wcout << L"Direct Mode " << (turnOn ? L"ON" : L"OFF") << L" command sent.\n";
            return 0;
        }
    }

    if (!has_property(properties, GattCharacteristicProperties::Write) &&
        !has_property(properties, GattCharacteristicProperties::WriteWithoutResponse))
    {
        std::wcerr << L"The characteristic does not advertise a writable property.\n";
        return 40;
    }

    std::wcerr << L"BLE write failed: " << status_name(lastStatus) << L"\n";
    return 41;
}
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2)
    {
        std::wcerr << L"Usage: x4-poc.exe on|off\n";
        return 2;
    }

    std::wstring command = argv[1];
    std::transform(command.begin(), command.end(), command.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });

    bool turnOn = false;
    if (command == L"on")
    {
        turnOn = true;
    }
    else if (command != L"off")
    {
        std::wcerr << L"Usage: x4-poc.exe on|off\n";
        return 2;
    }

    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        return run(turnOn);
    }
    catch (winrt::hresult_error const& error)
    {
        std::wcerr << L"WinRT error 0x" << std::hex << std::uppercase
                   << static_cast<uint32_t>(error.code().value)
                   << L": " << error.message().c_str() << L"\n";
        return 100;
    }
    catch (std::exception const& error)
    {
        std::cerr << "Error: " << error.what() << "\n";
        return 101;
    }
}
