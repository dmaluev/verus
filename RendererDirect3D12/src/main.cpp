// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

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
	VERUS_DLL_EXPORT verus::CGI::PBaseRenderer CreateRenderer(UINT32 version, verus::CGI::PBaseRendererDesc pDesc)
	{
		using namespace verus;

		if (VERUS_SDK_VERSION != version)
		{
			VERUS_RT_FAIL("CreateRenderer(), Wrong version.");
			return nullptr;
		}

		pDesc->_gvc.Paste();

		CGI::RendererD3D12::Make();
		VERUS_QREF_RENDERER_D3D12;

		pRendererD3D12->SetDesc(*pDesc);
		pRendererD3D12->Init();

		return pRendererD3D12;
	}
}
