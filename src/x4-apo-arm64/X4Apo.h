#pragma once

#include <atlbase.h>
#include <atlcom.h>
#include <audioenginebaseapo.h>
#include <audioengineextensionapo.h>
#include <baseaudioprocessingobject.h>
#include <mmdeviceapi.h>

// Official SB1815/X4 Windows 11 APO CLSIDs recovered from ctusbaud.inf.
extern const CLSID CLSID_X4_SFX;
extern const CLSID CLSID_X4_MFX;
extern const CLSID CLSID_X4_EFX;

// Stage A0 deliberately implements no DSP and exposes no controllable effects.
// The sole purpose is to prove native ARM64 APO graph loading with bit-transparent
// pass-through processing before any Creative property-store or DSP behavior is added.
class CX4PassThroughApoBase :
    public CBaseAudioProcessingObject,
    public IAudioSystemEffects3,
    public IAudioProcessingObjectNotifications
{
public:
    explicit CX4PassThroughApoBase(const APO_REG_PROPERTIES* registrationProperties);
    virtual ~CX4PassThroughApoBase() = default;

    STDMETHOD_(void, APOProcess)(
        UINT32 inputConnectionCount,
        APO_CONNECTION_PROPERTY** inputConnections,
        UINT32 outputConnectionCount,
        APO_CONNECTION_PROPERTY** outputConnections) override;

    STDMETHOD(Initialize)(UINT32 dataSize, BYTE* data) override;

    // IAudioSystemEffects2 / IAudioSystemEffects3.
    STDMETHOD(GetEffectsList)(
        LPGUID* effectIds,
        UINT* effectCount,
        HANDLE effectsChangedEvent) override;

    STDMETHOD(GetControllableSystemEffectsList)(
        AUDIO_SYSTEMEFFECT** effects,
        UINT* effectCount,
        HANDLE effectsChangedEvent) override;

    STDMETHOD(SetAudioSystemEffectState)(
        GUID effectId,
        AUDIO_SYSTEMEFFECT_STATE state) override;

    // IAudioProcessingObjectNotifications.
    STDMETHOD(GetApoNotificationRegistrationInfo)(
        APO_NOTIFICATION_DESCRIPTOR** notifications,
        DWORD* notificationCount) override;

    STDMETHOD_(void, HandleNotification)(APO_NOTIFICATION* notification) override;

protected:
    GUID m_processingMode = AUDIO_SIGNALPROCESSINGMODE_DEFAULT;
    CComPtr<IMMDevice> m_endpoint;
};

class CX4SfxApo final :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CX4SfxApo, &CLSID_X4_SFX>,
    public CX4PassThroughApoBase
{
public:
    CX4SfxApo();

    DECLARE_NO_REGISTRY()

    BEGIN_COM_MAP(CX4SfxApo)
        COM_INTERFACE_ENTRY(IAudioSystemEffects)
        COM_INTERFACE_ENTRY(IAudioSystemEffects2)
        COM_INTERFACE_ENTRY(IAudioSystemEffects3)
        COM_INTERFACE_ENTRY(IAudioProcessingObjectNotifications)
        COM_INTERFACE_ENTRY(IAudioProcessingObjectRT)
        COM_INTERFACE_ENTRY(IAudioProcessingObject)
        COM_INTERFACE_ENTRY(IAudioProcessingObjectConfiguration)
    END_COM_MAP()

    static const CRegAPOProperties<1> sm_RegProperties;
};

class CX4MfxApo final :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CX4MfxApo, &CLSID_X4_MFX>,
    public CX4PassThroughApoBase
{
public:
    CX4MfxApo();

    DECLARE_NO_REGISTRY()

    BEGIN_COM_MAP(CX4MfxApo)
        COM_INTERFACE_ENTRY(IAudioSystemEffects)
        COM_INTERFACE_ENTRY(IAudioSystemEffects2)
        COM_INTERFACE_ENTRY(IAudioSystemEffects3)
        COM_INTERFACE_ENTRY(IAudioProcessingObjectNotifications)
        COM_INTERFACE_ENTRY(IAudioProcessingObjectRT)
        COM_INTERFACE_ENTRY(IAudioProcessingObject)
        COM_INTERFACE_ENTRY(IAudioProcessingObjectConfiguration)
    END_COM_MAP()

    static const CRegAPOProperties<1> sm_RegProperties;
};

class CX4EfxApo final :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CX4EfxApo, &CLSID_X4_EFX>,
    public CX4PassThroughApoBase
{
public:
    CX4EfxApo();

    DECLARE_NO_REGISTRY()

    BEGIN_COM_MAP(CX4EfxApo)
        COM_INTERFACE_ENTRY(IAudioSystemEffects)
        COM_INTERFACE_ENTRY(IAudioSystemEffects2)
        COM_INTERFACE_ENTRY(IAudioSystemEffects3)
        COM_INTERFACE_ENTRY(IAudioProcessingObjectNotifications)
        COM_INTERFACE_ENTRY(IAudioProcessingObjectRT)
        COM_INTERFACE_ENTRY(IAudioProcessingObject)
        COM_INTERFACE_ENTRY(IAudioProcessingObjectConfiguration)
    END_COM_MAP()

    static const CRegAPOProperties<1> sm_RegProperties;
};

OBJECT_ENTRY_AUTO(CLSID_X4_SFX, CX4SfxApo)
OBJECT_ENTRY_AUTO(CLSID_X4_MFX, CX4MfxApo)
OBJECT_ENTRY_AUTO(CLSID_X4_EFX, CX4EfxApo)
