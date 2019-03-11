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
	VERUS_DLL_EXPORT void* VerusCreateRenderer(UINT32 version, void* pDesc)
	{
		return nullptr;
#if 0
		using namespace verus;

		if (VERUS_SDK_VERSION != version)
		{
			VERUS_RT_FAIL("FAIL: wrong version");
			return nullptr;
		}

		pDesc->_pack.Paste();

		CGI::RendererVulkan::Make();

		VERUS_QREF_RENDERER_VULKAN;

		pRendererVulkan->SetDesc(*pDesc);

		if (pDesc->m_createWindow)
			pRendererVulkan->InitWindow();

		pRendererVulkan->Init();

		return pRendererVulkan;
#endif
	}
}
