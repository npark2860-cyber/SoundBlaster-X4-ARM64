#include <atlbase.h>
#include <atlcom.h>

#include "X4Apo.h"

// Registration properties exported through the AudioBaseProcessingObject helper
// library. The three classes intentionally share the same pass-through core but
// keep distinct official SB1815 SFX/MFX/EFX CLSIDs and APO_REG_PROPERTIES.
APO_REG_PROPERTIES const* gCoreAPOs[] =
{
    &CX4SfxApo::sm_RegProperties.m_Properties,
    &CX4MfxApo::sm_RegProperties.m_Properties,
    &CX4EfxApo::sm_RegProperties.m_Properties,
};

class CX4ApoDllModule final : public CAtlDllModuleT<CX4ApoDllModule>
{
};

CX4ApoDllModule _AtlModule;

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID reserved)
{
    return _AtlModule.DllMain(reason, reserved);
}

__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow(void)
{
    return _AtlModule.DllCanUnloadNow();
}

_Check_return_
STDAPI DllGetClassObject(
    _In_ REFCLSID classId,
    _In_ REFIID interfaceId,
    _Outptr_ LPVOID* object)
{
    return _AtlModule.DllGetClassObject(classId, interfaceId, object);
}
