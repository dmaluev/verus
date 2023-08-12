// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

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

// ShaderD3D11:

ShaderD3D11::ShaderD3D11()
{
}

ShaderD3D11::~ShaderD3D11()
{
	Done();
}

void ShaderD3D11::Init(CSZ source, CSZ sourceName, CSZ* branches)
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;
	HRESULT hr = 0;

	_sourceName = sourceName;
	const size_t len = strlen(source);
	ShaderInclude inc;
	const String version = "5_0";
#ifdef _DEBUG
	const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_OPTIMIZATION_LEVEL1 | D3DCOMPILE_DEBUG;
#else
	const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
	ComPtr<ID3DBlob> pErrorMsgs;
	ComPtr<ID3DBlob> pBlob;

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
		String entry, stageEntries[+Stage::count], stages;
		Vector<String> vMacroName;
		Vector<String> vMacroValue;
		const String branch = Parse(*branches, entry, stageEntries, stages, vMacroName, vMacroValue, "DEF_");

		if (IsInIgnoreList(_C(branch)))
		{
			branches++;
			continue;
		}

		// <User defines>
		Vector<D3D_SHADER_MACRO> vDefines;
		vDefines.reserve(20);
		const int count = Utils::Cast32(vMacroName.size());
		VERUS_FOR(i, count)
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
		char defShaderQuality[64] = {};
		{
			sprintf_s(defShaderQuality, "%d", settings._gpuShaderQuality);
			vDefines.push_back({ "_SHADER_QUALITY", defShaderQuality });
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
		char defMaxNumBones[64] = {};
		{
			sprintf_s(defMaxNumBones, "%d", VERUS_MAX_BONES);
			vDefines.push_back({ "VERUS_MAX_BONES", defMaxNumBones });
		}
		vDefines.push_back({ "_DIRECT3D", "1" });
		vDefines.push_back({ "_DIRECT3D11", "1" });
		const int typeIndex = Utils::Cast32(vDefines.size());
		vDefines.push_back({ "_XS", "1" });
		vDefines.push_back({});
		// </System defines>

		Compiled compiled;
		compiled._entry = entry;

		if (strchr(_C(stages), 'V'))
		{
			compiled._stageCount++;
			vDefines[typeIndex].Name = "_VS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::vs]), _C("vs_" + version), flags, 0, &compiled._pBlobs[+Stage::vs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strchr(_C(stages), 'H'))
		{
			compiled._stageCount++;
			vDefines[typeIndex].Name = "_HS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::hs]), _C("hs_" + version), flags, 0, &compiled._pBlobs[+Stage::hs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strchr(_C(stages), 'D'))
		{
			compiled._stageCount++;
			vDefines[typeIndex].Name = "_DS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::ds]), _C("ds_" + version), flags, 0, &compiled._pBlobs[+Stage::ds], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strchr(_C(stages), 'G'))
		{
			compiled._stageCount++;
			vDefines[typeIndex].Name = "_GS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::gs]), _C("gs_" + version), flags, 0, &compiled._pBlobs[+Stage::gs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strchr(_C(stages), 'F'))
		{
			compiled._stageCount++;
			vDefines[typeIndex].Name = "_FS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::fs]), _C("ps_" + version), flags, 0, &compiled._pBlobs[+Stage::fs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strchr(_C(stages), 'C'))
		{
			compiled._stageCount++;
			vDefines[typeIndex].Name = "_CS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::cs]), _C("cs_" + version), flags, 0, &compiled._pBlobs[+Stage::cs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
			_compute = true;
		}

		_mapCompiled[branch] = compiled;

		branches++;
	}

	_vDescriptorSetDesc.reserve(8);
}

void ShaderD3D11::Done()
{
	for (auto& dsd : _vDescriptorSetDesc)
		dsd._pConstantBuffer.Reset();

	for (auto& [key, value] : _mapCompiled)
	{
		VERUS_FOR(i, +Stage::count)
			value._pBlobs[i].Reset();
	}

	VERUS_DONE(ShaderD3D11);
}

void ShaderD3D11::CreateDescriptorSet(int setNumber, const void* pSrc, int size, int capacity, std::initializer_list<Sampler> il, ShaderStageFlags stageFlags)
{
	VERUS_QREF_RENDERER_D3D11;
	VERUS_RT_ASSERT(_vDescriptorSetDesc.size() == setNumber);
	VERUS_RT_ASSERT(!(reinterpret_cast<intptr_t>(pSrc) & 0xF));
	HRESULT hr = 0;

	DescriptorSetDesc dsd;
	dsd._vSamplers.assign(il);
	dsd._pSrc = pSrc;
	dsd._size = size;
	dsd._alignedSize = size;
	dsd._capacity = 1;
	dsd._capacityInBytes = dsd._alignedSize * dsd._capacity;
	dsd._stageFlags = stageFlags;

	dsd._srvCount = 0;
	dsd._uavCount = 0;
	for (const auto& sampler : dsd._vSamplers)
	{
		if (Sampler::storageImage == sampler)
			dsd._uavCount++;
		else
			dsd._srvCount++;
	}
	dsd._srvStartSlot = 0;
	dsd._uavStartSlot = 0;
	for (const auto& prevDsd : _vDescriptorSetDesc)
	{
		dsd._srvStartSlot += prevDsd._srvCount;
		dsd._uavStartSlot += prevDsd._uavCount;
		if (!prevDsd._size) // Structured buffer?
			dsd._srvStartSlot++;
	}

	if (dsd._capacityInBytes)
	{
		D3D11_BUFFER_DESC bDesc = {};
		bDesc.ByteWidth = dsd._capacityInBytes;
		bDesc.Usage = D3D11_USAGE_DYNAMIC;
		bDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateBuffer(&bDesc, nullptr, &dsd._pConstantBuffer)))
			throw VERUS_RUNTIME_ERROR << "CreateBuffer(D3D11_BIND_CONSTANT_BUFFER); hr=" << VERUS_HR(hr);
		SetDebugObjectName(dsd._pConstantBuffer.Get(), _C("Shader.ConstantBuffer (" + _sourceName + ", set=" + std::to_string(setNumber) + ")"));
	}

	_vDescriptorSetDesc.push_back(dsd);
}

void ShaderD3D11::CreatePipelineLayout()
{
	// Everything is awesome, everything is cool...
}

CSHandle ShaderD3D11::BindDescriptorSetTextures(int setNumber, std::initializer_list<TexturePtr> il, const int* pMipLevels, const int* pArrayLayers)
{
	VERUS_QREF_RENDERER_D3D11;

	// <NewComplexSetHandle>
	int complexSetHandle = -1;
	VERUS_FOR(i, _vComplexSets.size())
	{
		if (_vComplexSets[i]._vTextures.empty())
		{
			complexSetHandle = i;
			break;
		}
	}
	if (-1 == complexSetHandle)
	{
		complexSetHandle = Utils::Cast32(_vComplexSets.size());
		_vComplexSets.resize(complexSetHandle + 1);
	}
	// </NewComplexSetHandle>

	const auto& dsd = _vDescriptorSetDesc[setNumber];
	VERUS_RT_ASSERT(dsd._vSamplers.size() == il.size());

	RComplexSet complexSet = _vComplexSets[complexSetHandle];
	complexSet._vTextures.reserve(il.size());
	complexSet._vSRVs.reserve(dsd._srvCount);
	complexSet._vUAVs.reserve(dsd._uavCount);
	int index = 0;
	for (const auto x : il)
	{
		complexSet._vTextures.push_back(x);
		const int mipLevelCount = x->GetMipLevelCount();
		const int mipLevel = pMipLevels ? pMipLevels[index] : 0;
		const int arrayLayer = pArrayLayers ? pArrayLayers[index] : 0;
		const auto& texD3D11 = static_cast<RTextureD3D11>(*x);

		if (Sampler::storageImage == dsd._vSamplers[index])
			complexSet._vUAVs.push_back(mipLevel >= 0 ? texD3D11.GetUAV(mipLevel + arrayLayer * (mipLevelCount - 1)) : nullptr);
		else
			complexSet._vSRVs.push_back(texD3D11.GetSRV());

		index++;
	}

	return CSHandle::Make(complexSetHandle);
}

void ShaderD3D11::FreeDescriptorSet(CSHandle& complexSetHandle)
{
	if (complexSetHandle.IsSet() && complexSetHandle.Get() < _vComplexSets.size())
	{
		auto& complexSet = _vComplexSets[complexSetHandle.Get()];
		complexSet._vTextures.clear();
		complexSet._vSRVs.clear();
		complexSet._vUAVs.clear();
	}
	complexSetHandle = CSHandle();
}

void ShaderD3D11::BeginBindDescriptors()
{
	// Map/Unmap is done per each update.
}

void ShaderD3D11::EndBindDescriptors()
{
}

ID3D11Buffer* ShaderD3D11::UpdateConstantBuffer(int setNumber) const
{
	VERUS_QREF_RENDERER_D3D11;
	HRESULT hr = 0;

	const auto& dsd = _vDescriptorSetDesc[setNumber];

	D3D11_MAPPED_SUBRESOURCE ms;
	if (FAILED(hr = pRendererD3D11->GetD3DDeviceContext()->Map(dsd._pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
		throw VERUS_RUNTIME_ERROR << "Map(); hr=" << VERUS_HR(hr);
	memcpy(ms.pData, dsd._pSrc, dsd._size);
	pRendererD3D11->GetD3DDeviceContext()->Unmap(dsd._pConstantBuffer.Get(), 0);

	return dsd._pConstantBuffer.Get();
}

ShaderStageFlags ShaderD3D11::GetShaderStageFlags(int setNumber) const
{
	const auto& dsd = _vDescriptorSetDesc[setNumber];
	return dsd._stageFlags;
}

void ShaderD3D11::GetShaderResources(int setNumber, int complexSetHandle, RShaderResources shaderResources) const
{
	const auto& dsd = _vDescriptorSetDesc[setNumber];
	shaderResources._srvCount = dsd._srvCount;
	shaderResources._uavCount = dsd._uavCount;
	shaderResources._srvStartSlot = dsd._srvStartSlot;
	shaderResources._uavStartSlot = dsd._uavStartSlot;
	if (complexSetHandle >= 0)
	{
		const auto& complexSet = _vComplexSets[complexSetHandle];
		if (shaderResources._srvCount)
			memcpy(shaderResources._srvs, complexSet._vSRVs.data(), shaderResources._srvCount * sizeof(ID3D11ShaderResourceView*));
		if (shaderResources._uavCount)
			memcpy(shaderResources._uavs, complexSet._vUAVs.data(), shaderResources._uavCount * sizeof(ID3D11UnorderedAccessView*));
	}
}

void ShaderD3D11::GetSamplers(int setNumber, int complexSetHandle, RShaderResources shaderResources) const
{
	VERUS_QREF_RENDERER_D3D11;

	const auto& dsd = _vDescriptorSetDesc[setNumber];
	VERUS_FOR(i, dsd._vSamplers.size())
	{
		if (Sampler::custom == dsd._vSamplers[i])
		{
			VERUS_RT_ASSERT(complexSetHandle >= 0);
			const auto& complexSet = _vComplexSets[complexSetHandle];
			const auto& texD3D11 = static_cast<RTextureD3D11>(*complexSet._vTextures[i]);
			shaderResources._samplers[i] = texD3D11.GetD3DSamplerState();
		}
		else if (Sampler::inputAttach == dsd._vSamplers[i])
		{
			shaderResources._samplers[i] = pRendererD3D11->GetD3DSamplerState(Sampler::nearestMipN);
		}
		else
		{
			shaderResources._samplers[i] = pRendererD3D11->GetD3DSamplerState(dsd._vSamplers[i]);
		}
	}
}

void ShaderD3D11::OnError(CSZ s) const
{
	VERUS_QREF_RENDERER;

	if (strstr(s, "HS': entrypoint not found"))
		return;
	if (strstr(s, "DS': entrypoint not found"))
		return;
	if (strstr(s, "GS': entrypoint not found"))
		return;
	if (strstr(s, "error X"))
		renderer.OnShaderError(s);
	else
		renderer.OnShaderWarning(s);
}
