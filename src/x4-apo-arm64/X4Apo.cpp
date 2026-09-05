#include <initguid.h>

#include "X4Apo.h"

// Creative X4/SB1815 APO CLSIDs recovered from ctusbaud.inf.
DEFINE_GUID(CLSID_X4_SFX,
    0x71dab6a1, 0x39f3, 0x423e, 0x90, 0xa8, 0x03, 0x27, 0x29, 0x85, 0x11, 0x57);
DEFINE_GUID(CLSID_X4_MFX,
    0xc624d7b2, 0x8333, 0x448e, 0x85, 0xc8, 0x51, 0xee, 0xfc, 0x20, 0x25, 0xed);
DEFINE_GUID(CLSID_X4_EFX,
    0xec2f4b76, 0x6ae1, 0x4db9, 0x8f, 0xf6, 0x34, 0x4b, 0x74, 0xcf, 0x96, 0x50);

const AVRT_DATA CRegAPOProperties<1> CX4SfxApo::sm_RegProperties(
    CLSID_X4_SFX,
    L"Sound Blaster X4 ARM64 Pass-through SFX",
    L"SoundBlaster-X4-ARM64 project",
    0,
    1,
    __uuidof(IAudioSystemEffects));

const AVRT_DATA CRegAPOProperties<1> CX4MfxApo::sm_RegProperties(
    CLSID_X4_MFX,
    L"Sound Blaster X4 ARM64 Pass-through MFX",
    L"SoundBlaster-X4-ARM64 project",
    0,
    1,
    __uuidof(IAudioSystemEffects));

const AVRT_DATA CRegAPOProperties<1> CX4EfxApo::sm_RegProperties(
    CLSID_X4_EFX,
    L"Sound Blaster X4 ARM64 Pass-through EFX",
    L"SoundBlaster-X4-ARM64 project",
    0,
    1,
    __uuidof(IAudioSystemEffects));

CX4PassThroughApoBase::CX4PassThroughApoBase(
    const APO_REG_PROPERTIES* registrationProperties)
    : CBaseAudioProcessingObject(registrationProperties)
{
}

CX4SfxApo::CX4SfxApo()
    : CX4PassThroughApoBase(&sm_RegProperties.m_Properties)
{
}

CX4MfxApo::CX4MfxApo()
    : CX4PassThroughApoBase(&sm_RegProperties.m_Properties)
{
}

CX4EfxApo::CX4EfxApo()
    : CX4PassThroughApoBase(&sm_RegProperties.m_Properties)
{
}

#pragma AVRT_CODE_BEGIN

void CX4PassThroughApoBase::APOProcess(
    UINT32 inputConnectionCount,
    APO_CONNECTION_PROPERTY** inputConnections,
    UINT32 outputConnectionCount,
    APO_CONNECTION_PROPERTY** outputConnections)
{
    // Stage A0 is deliberately transparent. Never allocate, block, log, open
    // properties or call COM from this real-time method.
    if (inputConnectionCount == 0 || outputConnectionCount == 0 ||
        inputConnections == nullptr || outputConnections == nullptr ||
        inputConnections[0] == nullptr || outputConnections[0] == nullptr)
    {
        return;
    }

    APO_CONNECTION_PROPERTY* input = inputConnections[0];
    APO_CONNECTION_PROPERTY* output = outputConnections[0];

    const UINT32 frameCount = input->u32ValidFrameCount;
    const UINT32 sampleCount = frameCount * GetSamplesPerFrame();

    FLOAT32* const inputSamples = reinterpret_cast<FLOAT32*>(input->pBuffer);
    FLOAT32* const outputSamples = reinterpret_cast<FLOAT32*>(output->pBuffer);

    switch (input->u32BufferFlags)
    {
    case BUFFER_SILENT:
        if (outputSamples != nullptr)
        {
            for (UINT32 i = 0; i < sampleCount; ++i)
            {
                outputSamples[i] = 0.0f;
            }
        }
        break;

    case BUFFER_VALID:
        if (outputSamples != nullptr && inputSamples != nullptr && outputSamples != inputSamples)
        {
            for (UINT32 i = 0; i < sampleCount; ++i)
            {
                outputSamples[i] = inputSamples[i];
            }
        }
        break;

    case BUFFER_INVALID:
    default:
        // BUFFER_INVALID is not expected from a valid locked graph. Do not
        // touch the sample buffer; propagate the flag/count below.
        break;
    }

    output->u32BufferFlags = input->u32BufferFlags;
    output->u32ValidFrameCount = frameCount;
}

#pragma AVRT_CODE_END

HRESULT CX4PassThroughApoBase::Initialize(UINT32 dataSize, BYTE* data)
{
    if ((data == nullptr && dataSize != 0) || (data != nullptr && dataSize == 0))
    {
        return E_INVALIDARG;
    }

    // Modern Windows 11 initialization. Stage A0 only records the processing
    // mode and endpoint identity; it intentionally does not open Creative's
    // User/Default/Volatile FX property stores yet because the same Creative
    // APO CLSIDs are used for both the general and headphone contexts.
    if (dataSize == sizeof(APOInitSystemEffects3) && data != nullptr)
    {
        auto* init = reinterpret_cast<APOInitSystemEffects3*>(data);
        m_processingMode = init->AudioProcessingMode;

        if (init->pDeviceCollection != nullptr)
        {
            UINT32 deviceCount = 0;
            if (SUCCEEDED(init->pDeviceCollection->GetCount(&deviceCount)) && deviceCount != 0)
            {
                // Microsoft SYSVAD documents the endpoint as the last device in
                // the APOInitSystemEffects3 device collection.
                (void)init->pDeviceCollection->Item(deviceCount - 1, &m_endpoint);
            }
        }
    }
    else if (dataSize == sizeof(APOInitSystemEffects2) && data != nullptr)
    {
        auto* init = reinterpret_cast<APOInitSystemEffects2*>(data);
        m_processingMode = init->AudioProcessingMode;
    }
    else if (dataSize == sizeof(APOInitSystemEffects) && data != nullptr)
    {
        m_processingMode = AUDIO_SIGNALPROCESSINGMODE_DEFAULT;
    }
    else
    {
        return E_INVALIDARG;
    }

    m_bIsInitialized = true;
    return S_OK;
}

HRESULT CX4PassThroughApoBase::GetEffectsList(
    LPGUID* effectIds,
    UINT* effectCount,
    HANDLE effectsChangedEvent)
{
    UNREFERENCED_PARAMETER(effectsChangedEvent);

    if (effectIds == nullptr || effectCount == nullptr)
    {
        return E_POINTER;
    }

    *effectIds = nullptr;
    *effectCount = 0;
    return S_OK;
}

HRESULT CX4PassThroughApoBase::GetControllableSystemEffectsList(
    AUDIO_SYSTEMEFFECT** effects,
    UINT* effectCount,
    HANDLE effectsChangedEvent)
{
    UNREFERENCED_PARAMETER(effectsChangedEvent);

    if (effects == nullptr || effectCount == nullptr)
    {
        return E_POINTER;
    }

    *effects = nullptr;
    *effectCount = 0;
    return S_OK;
}

HRESULT CX4PassThroughApoBase::SetAudioSystemEffectState(
    GUID effectId,
    AUDIO_SYSTEMEFFECT_STATE state)
{
    UNREFERENCED_PARAMETER(effectId);
    UNREFERENCED_PARAMETER(state);

    // Stage A0 advertises no controllable effects and never changes state.
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

HRESULT CX4PassThroughApoBase::GetApoNotificationRegistrationInfo(
    APO_NOTIFICATION_DESCRIPTOR** notifications,
    DWORD* notificationCount)
{
    if (notifications == nullptr || notificationCount == nullptr)
    {
        return E_POINTER;
    }

    // Property-change notification is intentionally deferred until the X4
    // context-selection path is implemented. Returning an empty list is valid
    // for the graph-loading/pass-through milestone.
    *notifications = nullptr;
    *notificationCount = 0;
    return S_OK;
}

void CX4PassThroughApoBase::HandleNotification(APO_NOTIFICATION* notification)
{
    UNREFERENCED_PARAMETER(notification);
}
