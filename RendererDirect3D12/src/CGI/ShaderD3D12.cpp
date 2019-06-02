#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

// ShaderInclude:

HRESULT STDMETHODCALLTYPE ShaderInclude::Open(
	D3D_INCLUDE_TYPE IncludeType,
	LPCSTR pFileName,
	LPCVOID pParentData,
	LPCVOID* ppData,
	UINT* pBytes)
{
	const String url = String("[Shaders]:") + pFileName;
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(_C(url), vData);
	char* p = new char[vData.size()];
	memcpy(p, vData.data(), vData.size());
	*pBytes = Utils::Cast32(vData.size());
	*ppData = p;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ShaderInclude::Close(LPCVOID pData)
{
	delete[] pData;
	return S_OK;
}

// ShaderD3D12:

ShaderD3D12::ShaderD3D12()
{
}

ShaderD3D12::~ShaderD3D12()
{
	Done();
}

void ShaderD3D12::Init(CSZ source, CSZ* branches)
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;
	HRESULT hr = 0;

	const size_t len = strlen(source);
	ShaderInclude inc;
	String version = "5_1";
#ifdef _DEBUG
	const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL1 | D3DCOMPILE_DEBUG;
#else
	const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
	ComPtr<ID3DBlob> pErrorMsgs;

	auto CheckErrorMsgs = [this](ComPtr<ID3DBlob>& pErrorMsgs)
	{
		if (pErrorMsgs)
		{
			OnError(static_cast<CSZ>(pErrorMsgs->GetBufferPointer()));
			pErrorMsgs.Reset();
		}
	};

	while (*branches)
	{
		String entryVS, entryHS, entryDS, entryGS, entryFS;
		Vector<String> vMacroName;
		Vector<String> vMacroValue;
		const String entry = Parse(*branches, entryVS, entryHS, entryDS, entryGS, entryFS, vMacroName, vMacroValue, "DEF_");

		if (IsInIgnoreList(_C(entry)))
		{
			branches++;
			continue;
		}

		// <User defines>
		Vector<D3D_SHADER_MACRO> vDefines;
		vDefines.reserve(20);
		const int num = Utils::Cast32(vMacroName.size());
		VERUS_FOR(i, num)
		{
			D3D_SHADER_MACRO sm;
			sm.Name = _C(vMacroName[i]);
			sm.Definition = _C(vMacroValue[i]);
			vDefines.push_back(sm);
		}
		// </User defines>

		// <System defines>
		char defAnisotropyLevel[64] = {};
		{
			sprintf_s(defAnisotropyLevel, "%d", settings._gpuAnisotropyLevel);
			vDefines.push_back({ "_ANISOTROPY_LEVEL", defAnisotropyLevel });
		}
		char defShadowQuality[64] = {};
		{
			sprintf_s(defShadowQuality, "%d", settings._sceneShadowQuality);
			vDefines.push_back({ "_SHADOW_QUALITY", defShadowQuality });
		}
		char defWaterQuality[64] = {};
		{
			sprintf_s(defWaterQuality, "%d", settings._sceneWaterQuality);
			vDefines.push_back({ "_WATER_QUALITY", defWaterQuality });
		}
		vDefines.push_back({ "_DIRECT3D", "1" });
		const int typeIndex = Utils::Cast32(vDefines.size());
		vDefines.push_back({ "_XS", "1" });
		vDefines.push_back({});
		// </System defines>

		Compiled compiled;

		if (strstr(source, " mainVS("))
		{
			compiled._numStages++;
			vDefines[typeIndex].Name = "_VS";
			hr = D3DCompile(source, len, 0, vDefines.data(), &inc, _C(entryVS), _C("vs_" + version), flags, 0, &compiled._pBlobs[+Stage::vs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strstr(source, " mainHS("))
		{
			compiled._numStages++;
			vDefines[typeIndex].Name = "_HS";
			hr = D3DCompile(source, len, 0, vDefines.data(), &inc, _C(entryHS), _C("hs_" + version), flags, 0, &compiled._pBlobs[+Stage::hs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strstr(source, " mainDS("))
		{
			compiled._numStages++;
			vDefines[typeIndex].Name = "_DS";
			hr = D3DCompile(source, len, 0, vDefines.data(), &inc, _C(entryDS), _C("ds_" + version), flags, 0, &compiled._pBlobs[+Stage::ds], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strstr(source, " mainGS("))
		{
			compiled._numStages++;
			vDefines[typeIndex].Name = "_GS";
			hr = D3DCompile(source, len, 0, vDefines.data(), &inc, _C(entryGS), _C("gs_" + version), flags, 0, &compiled._pBlobs[+Stage::gs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strstr(source, " mainFS("))
		{
			compiled._numStages++;
			vDefines[typeIndex].Name = "_FS";
			hr = D3DCompile(source, len, 0, vDefines.data(), &inc, _C(entryFS), _C("ps_" + version), flags, 0, &compiled._pBlobs[+Stage::fs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		_mapCompiled[entry] = compiled;

		branches++;
	}
}

void ShaderD3D12::Done()
{
	VERUS_DONE(ShaderD3D12);
}

void ShaderD3D12::OnError(CSZ s)
{
	VERUS_QREF_RENDERER;
	if (strstr(s, "HS': entrypoint not found"))
		return;
	if (strstr(s, "DS': entrypoint not found"))
		return;
	if (strstr(s, "GS': entrypoint not found"))
		return;
	if (strstr(s, ": error "))
		renderer.OnShaderError(s);
	else
		renderer.OnShaderWarning(s);
}
