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

void ShaderD3D12::Init(CSZ source, CSZ sourceName, CSZ* branches)
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
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(entryVS), _C("vs_" + version), flags, 0, &compiled._pBlobs[+Stage::vs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strstr(source, " mainHS("))
		{
			compiled._numStages++;
			vDefines[typeIndex].Name = "_HS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(entryHS), _C("hs_" + version), flags, 0, &compiled._pBlobs[+Stage::hs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strstr(source, " mainDS("))
		{
			compiled._numStages++;
			vDefines[typeIndex].Name = "_DS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(entryDS), _C("ds_" + version), flags, 0, &compiled._pBlobs[+Stage::ds], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strstr(source, " mainGS("))
		{
			compiled._numStages++;
			vDefines[typeIndex].Name = "_GS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(entryGS), _C("gs_" + version), flags, 0, &compiled._pBlobs[+Stage::gs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		if (strstr(source, " mainFS("))
		{
			compiled._numStages++;
			vDefines[typeIndex].Name = "_FS";
			hr = D3DCompile(source, len, sourceName, vDefines.data(), &inc, _C(entryFS), _C("ps_" + version), flags, 0, &compiled._pBlobs[+Stage::fs], &pErrorMsgs);
			CheckErrorMsgs(pErrorMsgs);
		}

		_mapCompiled[entry] = compiled;

		branches++;
	}

	_vUniformBuffers.reserve(4);
}

void ShaderD3D12::Done()
{
	for (auto& ub : _vUniformBuffers)
	{
		VERUS_COM_RELEASE_CHECK(ub._pConstantBuffer.Get());
		ub._pConstantBuffer.Reset();
	}

	VERUS_COM_RELEASE_CHECK(_pRootSignature.Get());
	_pRootSignature.Reset();

	for (auto& x : _mapCompiled)
	{
		VERUS_FOR(i, +Stage::count)
		{
			VERUS_COM_RELEASE_CHECK(x.second._pBlobs[i].Get());
			x.second._pBlobs[i].Reset();
		}
	}

	VERUS_DONE(ShaderD3D12);
}

void ShaderD3D12::BindUniformBufferSource(int descSet, const void* pSrc, int size, int capacity, ShaderStageFlags stageFlags)
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	VERUS_RT_ASSERT(!(reinterpret_cast<intptr_t>(pSrc) & 0xF));

	UniformBuffer uniformBuffer;
	uniformBuffer._pSrc = pSrc;
	uniformBuffer._size = size;
	uniformBuffer._sizeAligned = Math::AlignUp(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	uniformBuffer._capacity = capacity;
	uniformBuffer._capacityInBytes = uniformBuffer._sizeAligned*uniformBuffer._capacity;
	uniformBuffer._stageFlags = stageFlags;

	if (capacity > 0)
	{
		const UINT64 bufferSize = uniformBuffer._capacityInBytes * BaseRenderer::s_ringBufferSize;
		if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uniformBuffer._pConstantBuffer))))
			throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(), hr=" << VERUS_HR(hr);
	}

	if (_vUniformBuffers.size() <= descSet)
		_vUniformBuffers.resize(descSet + 1);
	_vUniformBuffers[descSet] = uniformBuffer;
}

void ShaderD3D12::CreatePipelineLayout()
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureSupportData = {};
	featureSupportData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(pRendererD3D12->GetD3DDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureSupportData, sizeof(featureSupportData))))
		featureSupportData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Vector<CD3DX12_DESCRIPTOR_RANGE1> vDescRanges;
	vDescRanges.reserve(_vUniformBuffers.size()); // Must not reallocate.
	Vector<CD3DX12_ROOT_PARAMETER1> vRootParams;
	vRootParams.reserve(_vUniformBuffers.size());
	if (!_vUniformBuffers.empty())
	{
		ShaderStageFlags stageFlags = _vUniformBuffers.front()._stageFlags;
		// Reverse order:
		const int num = Utils::Cast32(_vUniformBuffers.size());
		int space = num - 1;
		for (int i = num - 1; i >= 0; --i)
		{
			auto& ub = _vUniformBuffers[i];
			stageFlags |= ub._stageFlags;
			if (ub._capacity > 0)
			{
				CD3DX12_DESCRIPTOR_RANGE1 descRange;
				descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, space, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
				vDescRanges.push_back(descRange);
				CD3DX12_ROOT_PARAMETER1 rootParam;
				rootParam.InitAsDescriptorTable(1, &vDescRanges.back(), D3D12_SHADER_VISIBILITY_ALL);
				vRootParams.push_back(rootParam);
			}
			else
			{
				VERUS_RT_ASSERT(&ub == &_vUniformBuffers.back()); // Only the last set is allowed to use push constants.
				CD3DX12_ROOT_PARAMETER1 rootParam;
				rootParam.InitAsConstants(ub._size >> 2, 0, space);
				vRootParams.push_back(rootParam);
			}
			space--;
		}

		if (!(stageFlags & ShaderStageFlags::vs))
			rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
		if (!(stageFlags & ShaderStageFlags::hs))
			rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
		if (!(stageFlags & ShaderStageFlags::ds))
			rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
		if (!(stageFlags & ShaderStageFlags::gs))
			rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		if (!(stageFlags & ShaderStageFlags::fs))
			rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	}

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(Utils::Cast32(vRootParams.size()), vRootParams.data(), 0, nullptr, rootSignatureFlags);

	ComPtr<ID3DBlob> pRootSignatureBlob;
	ComPtr<ID3DBlob> pErrorBlob;
	if (FAILED(hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureSupportData.HighestVersion, &pRootSignatureBlob, &pErrorBlob)))
	{
		CSZ errors = pErrorBlob ? static_cast<CSZ>(pErrorBlob->GetBufferPointer()) : "";
		throw VERUS_RUNTIME_ERROR << "D3DX12SerializeVersionedRootSignature(), hr=" << VERUS_HR(hr) << ", errors=" << errors;
	}
	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateRootSignature(0, pRootSignatureBlob->GetBufferPointer(), pRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&_pRootSignature))))
		throw VERUS_RUNTIME_ERROR << "CreateRootSignature(), hr=" << VERUS_HR(hr);
}

void ShaderD3D12::BeginBindDescriptors()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	const bool resetOffset = _currentFrame != renderer.GetNumFrames();
	_currentFrame = renderer.GetNumFrames();

	for (auto& ub : _vUniformBuffers)
	{
		if (!ub._capacity)
			continue;
		VERUS_RT_ASSERT(!ub._pMappedData);
		CD3DX12_RANGE readRange(0, 0);
		void* pData = nullptr;
		if (FAILED(hr = ub._pConstantBuffer->Map(0, &readRange, &pData)))
			throw VERUS_RUNTIME_ERROR << "Map(), hr=" << VERUS_HR(hr);
		ub._pMappedData = static_cast<BYTE*>(pData);
		ub._pMappedData += ub._capacityInBytes*pRendererD3D12->GetRingBufferIndex(); // Adjust address for this frame.
		if (resetOffset)
			ub._offset = 0;
	}
}

void ShaderD3D12::EndBindDescriptors()
{
	for (auto& ub : _vUniformBuffers)
	{
		if (!ub._capacity)
			continue;
		VERUS_RT_ASSERT(ub._pMappedData);
		ub._pConstantBuffer->Unmap(0, nullptr);
		ub._pMappedData = nullptr;
	}
}

UINT ShaderD3D12::ToRootParameterIndex(int descSet) const
{
	return static_cast<UINT>(_vUniformBuffers.size()) - descSet - 1;
}

bool ShaderD3D12::TryRootConstants(int descSet, RBaseCommandBuffer cb)
{
	auto& ub = _vUniformBuffers[descSet];
	if (!ub._capacity)
	{
		auto& cbD3D12 = static_cast<RCommandBufferD3D12>(cb);
		const UINT rootParameterIndex = ToRootParameterIndex(descSet);
		cbD3D12.GetD3DGraphicsCommandList()->SetGraphicsRoot32BitConstants(rootParameterIndex, ub._size >> 2, ub._pSrc, 0);
		return true;
	}
	return false;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShaderD3D12::UpdateUniformBuffer(int descSet)
{
	VERUS_QREF_RENDERER_D3D12;

	auto& ub = _vUniformBuffers[descSet];
	if (ub._offset + ub._sizeAligned > ub._capacityInBytes)
		return CD3DX12_GPU_DESCRIPTOR_HANDLE();

	VERUS_RT_ASSERT(ub._pMappedData);
	const int at = ub._capacityInBytes * pRendererD3D12->GetRingBufferIndex() + ub._offset;
	memcpy(ub._pMappedData + ub._offset, ub._pSrc, ub._size);
	ub._offset += ub._sizeAligned;

	auto handle = pRendererD3D12->GetHeapCbvSrvUav().GetNextHandle();
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.BufferLocation = ub._pConstantBuffer->GetGPUVirtualAddress() + at;
	desc.SizeInBytes = ub._sizeAligned;
	pRendererD3D12->GetD3DDevice()->CreateConstantBufferView(&desc, handle.first);
	return handle.second;
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
	if (strstr(s, "error X"))
		renderer.OnShaderError(s);
	else
		renderer.OnShaderWarning(s);
}
