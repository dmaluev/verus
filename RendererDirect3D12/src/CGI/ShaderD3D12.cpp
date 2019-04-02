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
	const String url = String("<Shaders>/") + pFileName;
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(_C(url), vData);
	char* p = new char[vData.size()];
	memcpy(p, vData.data(), vData.size());
	*pBytes = vData.size();
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

void ShaderD3D12::Init(CSZ source, CSZ* list)
{
	VERUS_INIT();

	HRESULT hr = 0;

	const size_t len = strlen(source);
	ShaderInclude inc;
	String version = "5_0";
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef _DEBUG
	flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
	ComPtr<ID3DBlob> pErrorMsgs;

	Vector<D3D_SHADER_MACRO> vDefines;
	vDefines.reserve(20);

	while (*list)
	{
		String entryVS, entryHS, entryDS, entryGS, entryPS;
		const String entry;

		Compiled compiled;

		hr = D3DCompile(source, len, 0, vDefines.data(), &inc, _C(entryVS), _C("vs_" + version), flags, 0, &compiled._pVS, &pErrorMsgs);

		_mapCompiled[entry] = compiled;

		list++;
	}
}

void ShaderD3D12::Done()
{
	VERUS_DONE(ShaderD3D12);
}
