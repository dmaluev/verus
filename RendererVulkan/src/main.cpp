#include "stdafx.h"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

extern "C"
{
	VERUS_DLL_EXPORT verus::CGI::PBaseRenderer VerusCreateRenderer(UINT32 version, verus::CGI::PBaseRendererDesc pDesc)
	{
		using namespace verus;

		if (VERUS_SDK_VERSION != version)
		{
			VERUS_RT_FAIL("VerusCreateRenderer(), Wrong version");
			return nullptr;
		}

		pDesc->_gvc.Paste();

		CGI::RendererVulkan::Make();
		VERUS_QREF_RENDERER_VULKAN;

		pRendererVulkan->SetDesc(*pDesc);
		pRendererVulkan->Init();

		return pRendererVulkan;
	}
}
