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

	_sourceName = sourceName;
	const size_t len = strlen(source);
	ShaderInclude inc;
	const String version = "5_1";
#ifdef _DEBUG
	const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_OPTIMIZATION_LEVEL1 | D3DCOMPILE_DEBUG;
#else
	const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_OPTIMIZATION_LEVEL3;
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
		vDefines.push_back({ "_DIRECT3D12", "1" });
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

void ShaderD3D12::Done()
{
	for (auto& dsd : _vDescriptorSetDesc)
	{
		VERUS_SMART_RELEASE(dsd._pMaAllocation);
		VERUS_COM_RELEASE_CHECK(dsd._pConstantBuffer.Get());
		dsd._dhDynamicOffsets.Reset();
		dsd._pConstantBuffer.Reset();
	}

	_pRootSignature.Reset(); // Can be shared with another shader, don't check ref count.

	for (auto& [key, value] : _mapCompiled)
	{
		VERUS_FOR(i, +Stage::count)
		{
			VERUS_COM_RELEASE_CHECK(value._pBlobs[i].Get());
			value._pBlobs[i].Reset();
		}
	}

	VERUS_DONE(ShaderD3D12);
}

void ShaderD3D12::CreateDescriptorSet(int setNumber, const void* pSrc, int size, int capacity, std::initializer_list<Sampler> il, ShaderStageFlags stageFlags)
{
	VERUS_QREF_RENDERER_D3D12;
	VERUS_RT_ASSERT(_vDescriptorSetDesc.size() == setNumber);
	VERUS_RT_ASSERT(!(reinterpret_cast<intptr_t>(pSrc) & 0xF));
	HRESULT hr = 0;

	DescriptorSetDesc dsd;
	dsd._vSamplers.assign(il);
	dsd._pSrc = pSrc;
	dsd._size = size;
	dsd._alignedSize = Math::AlignUp(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	dsd._capacity = capacity;
	dsd._capacityInBytes = dsd._alignedSize * dsd._capacity;
	dsd._stageFlags = stageFlags;

	if (capacity > 0)
	{
		const UINT64 bufferSize = dsd._capacityInBytes * BaseRenderer::s_ringBufferSize;
		D3D12MA::ALLOCATION_DESC allocDesc = {};
		allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
			&allocDesc,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			&dsd._pMaAllocation,
			IID_PPV_ARGS(&dsd._pConstantBuffer))))
			throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_UPLOAD); hr=" << VERUS_HR(hr);
		dsd._pConstantBuffer->SetName(_C(Str::Utf8ToWide("Shader.ConstantBuffer (" + _sourceName + ", set=" + std::to_string(setNumber) + ")")));

		// Device requires alignment be a multiple of 256.
		// Device requires SizeInBytes be a multiple of 256.
		const int count = dsd._capacity * BaseRenderer::s_ringBufferSize;
		dsd._dhDynamicOffsets.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, count);
		VERUS_FOR(i, count)
		{
			const int offset = i * dsd._alignedSize;
			auto handle = dsd._dhDynamicOffsets.AtCPU(i);
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
			desc.BufferLocation = dsd._pConstantBuffer->GetGPUVirtualAddress() + offset;
			desc.SizeInBytes = dsd._alignedSize;
			pRendererD3D12->GetD3DDevice()->CreateConstantBufferView(&desc, handle);
		}
	}

	_vDescriptorSetDesc.push_back(dsd);
}

void ShaderD3D12::CreatePipelineLayout()
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	int allTextureCount = 0;
	for (const auto& dsd : _vDescriptorSetDesc)
		allTextureCount += Utils::Cast32(dsd._vSamplers.size());

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureSupportData = {};
	featureSupportData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(pRendererD3D12->GetD3DDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureSupportData, sizeof(featureSupportData))))
		featureSupportData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = _compute ?
		D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Vector<CD3DX12_DESCRIPTOR_RANGE1> vDescRanges;
	vDescRanges.reserve(_vDescriptorSetDesc.size() + allTextureCount); // Must not reallocate.
	Vector<CD3DX12_DESCRIPTOR_RANGE1> vDescRangesSamplers;
	vDescRangesSamplers.reserve(allTextureCount);
	Vector<CD3DX12_ROOT_PARAMETER1> vRootParams;
	vRootParams.reserve(_vDescriptorSetDesc.size() + 1);
	Vector<D3D12_STATIC_SAMPLER_DESC> vStaticSamplers;
	vStaticSamplers.reserve(allTextureCount);
	if (!_vDescriptorSetDesc.empty())
	{
		ShaderStageFlags stageFlags = _vDescriptorSetDesc.front()._stageFlags;
		// Reverse order:
		const int count = Utils::Cast32(_vDescriptorSetDesc.size());
		int space = count - 1;
		for (int i = count - 1; i >= 0; --i)
		{
			auto& dsd = _vDescriptorSetDesc[i];
			stageFlags |= dsd._stageFlags;

			D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
			switch (dsd._stageFlags)
			{
			case ShaderStageFlags::vs: visibility = D3D12_SHADER_VISIBILITY_VERTEX; break;
			case ShaderStageFlags::hs: visibility = D3D12_SHADER_VISIBILITY_HULL; break;
			case ShaderStageFlags::ds: visibility = D3D12_SHADER_VISIBILITY_DOMAIN; break;
			case ShaderStageFlags::gs: visibility = D3D12_SHADER_VISIBILITY_GEOMETRY; break;
			case ShaderStageFlags::fs: visibility = D3D12_SHADER_VISIBILITY_PIXEL; break;
			case ShaderStageFlags::ts: visibility = D3D12_SHADER_VISIBILITY_AMPLIFICATION; break;
			case ShaderStageFlags::ms: visibility = D3D12_SHADER_VISIBILITY_MESH; break;
			}

			if (dsd._capacity > 0)
			{
				CD3DX12_DESCRIPTOR_RANGE1 descRange;
				descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, space);
				vDescRanges.push_back(descRange);
				const auto pDescriptorRanges = &vDescRanges.back();

				const int textureCount = Utils::Cast32(dsd._vSamplers.size());
				VERUS_FOR(i, textureCount)
				{
					if (Sampler::storageImage == dsd._vSamplers[i])
					{
						CD3DX12_DESCRIPTOR_RANGE1 descRange;
						descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1 + i, space);
						vDescRanges.push_back(descRange);
					}
					else
					{
						CD3DX12_DESCRIPTOR_RANGE1 descRange;
						descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1 + i, space);
						vDescRanges.push_back(descRange);
						if (Sampler::custom == dsd._vSamplers[i])
						{
							dsd._staticSamplersOnly = false;
							CD3DX12_DESCRIPTOR_RANGE1 descRangeSampler;
							descRangeSampler.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 1 + i, space);
							vDescRangesSamplers.push_back(descRangeSampler);
						}
						else
						{
							Sampler s = dsd._vSamplers[i];
							if (Sampler::inputAttach == s)
								s = Sampler::nearestMipN;
							D3D12_STATIC_SAMPLER_DESC samplerDesc = pRendererD3D12->GetD3DStaticSamplerDesc(s);
							samplerDesc.ShaderRegister = 1 + i;
							samplerDesc.RegisterSpace = space;
							samplerDesc.ShaderVisibility = visibility;
							vStaticSamplers.push_back(samplerDesc);
						}
					}
				}

				CD3DX12_ROOT_PARAMETER1 rootParam;
				rootParam.InitAsDescriptorTable(1 + textureCount, pDescriptorRanges, visibility);
				vRootParams.push_back(rootParam);
			}
			else if (!dsd._size) // Structured buffer?
			{
				CD3DX12_DESCRIPTOR_RANGE1 descRange;
				descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, space);
				vDescRanges.push_back(descRange);

				CD3DX12_ROOT_PARAMETER1 rootParam;
				rootParam.InitAsDescriptorTable(1, &vDescRanges.back(), visibility);
				vRootParams.push_back(rootParam);
			}
			else
			{
				VERUS_RT_ASSERT(&dsd == &_vDescriptorSetDesc.back()); // Only the last set is allowed to use push constants.
				CD3DX12_ROOT_PARAMETER1 rootParam;
				rootParam.InitAsConstants(dsd._size >> 2, 0, space);
				vRootParams.push_back(rootParam);
			}
			space--;
		}

		// Samplers:
		if (!vDescRangesSamplers.empty())
		{
			CD3DX12_ROOT_PARAMETER1 rootParam;
			rootParam.InitAsDescriptorTable(Utils::Cast32(vDescRangesSamplers.size()), vDescRangesSamplers.data());
			vRootParams.push_back(rootParam);
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
		if (!(stageFlags & ShaderStageFlags::ts))
			rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;
		if (!(stageFlags & ShaderStageFlags::ms))
			rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
	}

	UpdateDebugInfo(vRootParams, vStaticSamplers, rootSignatureFlags);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(
		Utils::Cast32(vRootParams.size()), vRootParams.data(),
		Utils::Cast32(vStaticSamplers.size()), vStaticSamplers.data(),
		rootSignatureFlags);

	ComPtr<ID3DBlob> pRootSignatureBlob;
	ComPtr<ID3DBlob> pErrorBlob;
	if (FAILED(hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureSupportData.HighestVersion, &pRootSignatureBlob, &pErrorBlob)))
	{
		CSZ errors = pErrorBlob ? static_cast<CSZ>(pErrorBlob->GetBufferPointer()) : "";
		throw VERUS_RUNTIME_ERROR << "D3DX12SerializeVersionedRootSignature(); hr=" << VERUS_HR(hr) << ", errors=" << errors;
	}
	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateRootSignature(0, pRootSignatureBlob->GetBufferPointer(), pRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&_pRootSignature))))
		throw VERUS_RUNTIME_ERROR << "CreateRootSignature(); hr=" << VERUS_HR(hr);
	_pRootSignature->SetName(_C(Str::Utf8ToWide("RootSignature (" + _sourceName + ")")));
}

CSHandle ShaderD3D12::BindDescriptorSetTextures(int setNumber, std::initializer_list<TexturePtr> il, const int* pMipLevels, const int* pArrayLayers)
{
	VERUS_QREF_RENDERER_D3D12;

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

	bool toViewHeap = false;
	if (setNumber & ShaderD3D12::SET_MOD_TO_VIEW_HEAP)
	{
		setNumber &= ~ShaderD3D12::SET_MOD_TO_VIEW_HEAP;
		toViewHeap = true;
	}

	const auto& dsd = _vDescriptorSetDesc[setNumber];
	VERUS_RT_ASSERT(dsd._vSamplers.size() == il.size());

	RComplexSet complexSet = _vComplexSets[complexSetHandle];
	complexSet._vTextures.reserve(il.size());
	if (toViewHeap) // Copy directly to view heap? This will work only during this frame.
		complexSet._hpBase = pRendererD3D12->GetViewHeap().GetNextHandlePair(1 + Utils::Cast32(il.size()));
	else // Create yet another descriptor heap? There is a limit on their number.
		complexSet._dhViews.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Utils::Cast32(il.size()));
	if (!dsd._staticSamplersOnly)
		complexSet._dhSamplers.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, Utils::Cast32(il.size()));
	int index = 0;
	for (auto x : il)
	{
		complexSet._vTextures.push_back(x);
		const int mipLevelCount = x->GetMipLevelCount();
		const int mipLevel = pMipLevels ? pMipLevels[index] : 0;
		const int arrayLayer = pArrayLayers ? pArrayLayers[index] : 0;
		auto& texD3D12 = static_cast<RTextureD3D12>(*x);

		if (Sampler::storageImage == dsd._vSamplers[index])
		{
			if (toViewHeap)
			{
				pRendererD3D12->GetD3DDevice()->CopyDescriptorsSimple(1,
					CD3DX12_CPU_DESCRIPTOR_HANDLE(complexSet._hpBase._hCPU, 1 + index, pRendererD3D12->GetViewHeap().GetHandleIncrementSize()),
					texD3D12.GetDescriptorHeapUAV().AtCPU(mipLevel + arrayLayer * (mipLevelCount - 1)),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
			else
			{
				pRendererD3D12->GetD3DDevice()->CopyDescriptorsSimple(1,
					complexSet._dhViews.AtCPU(index),
					texD3D12.GetDescriptorHeapUAV().AtCPU(mipLevel + arrayLayer * (mipLevelCount - 1)),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
		else
		{
			if (toViewHeap)
			{
				pRendererD3D12->GetD3DDevice()->CopyDescriptorsSimple(1,
					CD3DX12_CPU_DESCRIPTOR_HANDLE(complexSet._hpBase._hCPU, 1 + index, pRendererD3D12->GetViewHeap().GetHandleIncrementSize()),
					texD3D12.GetDescriptorHeapSRV().AtCPU(0),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
			else
			{
				pRendererD3D12->GetD3DDevice()->CopyDescriptorsSimple(1,
					complexSet._dhViews.AtCPU(index),
					texD3D12.GetDescriptorHeapSRV().AtCPU(0),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
			if (Sampler::custom == dsd._vSamplers[index])
			{
				pRendererD3D12->GetD3DDevice()->CopyDescriptorsSimple(1,
					complexSet._dhSamplers.AtCPU(index),
					texD3D12.GetDescriptorHeapSampler().AtCPU(0),
					D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
		}

		index++;
	}

	return CSHandle::Make(complexSetHandle);
}

void ShaderD3D12::FreeDescriptorSet(CSHandle& complexSetHandle)
{
	if (complexSetHandle.IsSet() && complexSetHandle.Get() < _vComplexSets.size())
	{
		auto& complexSet = _vComplexSets[complexSetHandle.Get()];
		complexSet._vTextures.clear();
		complexSet._dhViews.Reset();
		complexSet._dhSamplers.Reset();
		complexSet._hpBase = HandlePair();
	}
	complexSetHandle = CSHandle();
}

void ShaderD3D12::BeginBindDescriptors()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	const bool resetOffset = _currentFrame != renderer.GetFrameCount();
	_currentFrame = renderer.GetFrameCount();

	for (auto& dsd : _vDescriptorSetDesc)
	{
		if (!dsd._capacity)
			continue;
		VERUS_RT_ASSERT(!dsd._pMappedData);
		CD3DX12_RANGE readRange(0, 0);
		void* pData = nullptr;
		if (FAILED(hr = dsd._pConstantBuffer->Map(0, &readRange, &pData)))
			throw VERUS_RUNTIME_ERROR << "Map(); hr=" << VERUS_HR(hr);
		dsd._pMappedData = static_cast<BYTE*>(pData);
		dsd._pMappedData += dsd._capacityInBytes * pRendererD3D12->GetRingBufferIndex(); // Adjust address for this frame.
		if (resetOffset)
		{
			dsd._offset = 0;
			dsd._index = 0;
		}
	}
}

void ShaderD3D12::EndBindDescriptors()
{
	for (auto& dsd : _vDescriptorSetDesc)
	{
		if (!dsd._capacity)
			continue;
		VERUS_RT_ASSERT(dsd._pMappedData);
		dsd._pConstantBuffer->Unmap(0, nullptr);
		dsd._pMappedData = nullptr;
	}
}

UINT ShaderD3D12::ToRootParameterIndex(int setNumber) const
{
	// Arrange parameters in a large root signature so that the parameters most likely to change often,
	// or if low access latency for a given parameter is important, occur first.
	return static_cast<UINT>(_vDescriptorSetDesc.size()) - setNumber - 1;
}

bool ShaderD3D12::TryRootConstants(int setNumber, RBaseCommandBuffer cb) const
{
	const auto& dsd = _vDescriptorSetDesc[setNumber];
	if (!dsd._capacity)
	{
		auto& cbD3D12 = static_cast<RCommandBufferD3D12>(cb);
		const UINT rootParameterIndex = ToRootParameterIndex(setNumber);
		if (_compute)
			cbD3D12.GetD3DGraphicsCommandList()->SetComputeRoot32BitConstants(rootParameterIndex, dsd._size >> 2, dsd._pSrc, 0);
		else
			cbD3D12.GetD3DGraphicsCommandList()->SetGraphicsRoot32BitConstants(rootParameterIndex, dsd._size >> 2, dsd._pSrc, 0);
		return true;
	}
	return false;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShaderD3D12::UpdateConstantBuffer(int setNumber, int complexSetHandle)
{
	VERUS_QREF_RENDERER_D3D12;

	auto& dsd = _vDescriptorSetDesc[setNumber];
	if (dsd._offset + dsd._alignedSize > dsd._capacityInBytes)
	{
		VERUS_RT_FAIL("ConstantBuffer is full.");
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	}

	VERUS_RT_ASSERT(dsd._pMappedData);
	const int at = dsd._capacity * pRendererD3D12->GetRingBufferIndex() + dsd._index;
	memcpy(dsd._pMappedData + dsd._offset, dsd._pSrc, dsd._size);
	dsd._offset += dsd._alignedSize;
	dsd._peakLoad = Math::Max(dsd._peakLoad, dsd._offset);
	dsd._index++;

	bool hpBaseUsed = false;
	HandlePair hpBase;
	if (complexSetHandle >= 0)
	{
		const auto& complexSet = _vComplexSets[complexSetHandle];
		if (!complexSet._dhViews.GetD3DDescriptorHeap())
		{
			hpBase = complexSet._hpBase;
			hpBaseUsed = true;
		}
	}
	if (!hpBaseUsed)
		hpBase = pRendererD3D12->GetViewHeap().GetNextHandlePair();

	// Copy CBV:
	pRendererD3D12->GetD3DDevice()->CopyDescriptorsSimple(1,
		hpBase._hCPU,
		dsd._dhDynamicOffsets.AtCPU(at),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	if (complexSetHandle >= 0 && !hpBaseUsed)
	{
		// Copy SRVs:
		const auto& complexSet = _vComplexSets[complexSetHandle];
		const int count = Utils::Cast32(complexSet._vTextures.size());
		pRendererD3D12->GetD3DDevice()->CopyDescriptorsSimple(count,
			pRendererD3D12->GetViewHeap().GetNextHandlePair(count)._hCPU,
			complexSet._dhViews.AtCPU(0),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	return hpBase._hGPU;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShaderD3D12::UpdateSamplers(int setNumber, int complexSetHandle) const
{
	VERUS_QREF_RENDERER_D3D12;
	VERUS_RT_ASSERT(complexSetHandle >= 0);

	const auto& dsd = _vDescriptorSetDesc[setNumber];
	if (dsd._staticSamplersOnly)
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);

	CD3DX12_GPU_DESCRIPTOR_HANDLE hOffset(D3D12_DEFAULT);
	const auto& complexSet = _vComplexSets[complexSetHandle];
	const int maxCount = Utils::Cast32(complexSet._vTextures.size());
	VERUS_FOR(i, maxCount)
	{
		if (Sampler::custom == dsd._vSamplers[i])
		{
			auto hpSampler = pRendererD3D12->GetSamplerHeap().GetNextHandlePair(1);
			if (!hOffset.ptr)
				hOffset = hpSampler._hGPU;
			pRendererD3D12->GetD3DDevice()->CopyDescriptorsSimple(1,
				hpSampler._hCPU,
				complexSet._dhSamplers.AtCPU(i),
				D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}
	}

	return hOffset;
}

void ShaderD3D12::OnError(CSZ s) const
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

void ShaderD3D12::UpdateDebugInfo(
	const Vector<CD3DX12_ROOT_PARAMETER1>& vRootParams,
	const Vector<D3D12_STATIC_SAMPLER_DESC>& vStaticSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags)
{
	StringStream ss;

	auto PrintShaderVisibility = [&ss](D3D12_SHADER_VISIBILITY shaderVisibility)
		{
			switch (shaderVisibility)
			{
			case D3D12_SHADER_VISIBILITY_ALL: ss << "All"; break;
			case D3D12_SHADER_VISIBILITY_VERTEX: ss << "V"; break;
			case D3D12_SHADER_VISIBILITY_HULL: ss << "H"; break;
			case D3D12_SHADER_VISIBILITY_DOMAIN: ss << "D"; break;
			case D3D12_SHADER_VISIBILITY_GEOMETRY: ss << "G"; break;
			case D3D12_SHADER_VISIBILITY_PIXEL: ss << "P"; break;
			case D3D12_SHADER_VISIBILITY_AMPLIFICATION: ss << "A"; break;
			case D3D12_SHADER_VISIBILITY_MESH: ss << "M"; break;
			}
		};

	int totalSize = 0;
	int index = 0;
	for (const auto& rootParam : vRootParams)
	{
		ss << "Index=" << index;
		ss << ", Type=";
		switch (rootParam.ParameterType)
		{
		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: ss << "Table"; totalSize += 4; break;
		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: ss << "Const"; totalSize += 4 * rootParam.Constants.Num32BitValues; break;
		case D3D12_ROOT_PARAMETER_TYPE_CBV: ss << "CBV"; totalSize += 8; break;
		case D3D12_ROOT_PARAMETER_TYPE_SRV: ss << "SRV"; totalSize += 8; break;
		case D3D12_ROOT_PARAMETER_TYPE_UAV: ss << "UAV"; totalSize += 8; break;
		}
		ss << ", Visibility=";
		PrintShaderVisibility(rootParam.ShaderVisibility);
		if (totalSize > 64)
			ss << " (spilled)";
		ss << std::endl;
		switch (rootParam.ParameterType)
		{
		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
		{
			VERUS_U_FOR(i, rootParam.DescriptorTable.NumDescriptorRanges)
			{
				auto& descRange = rootParam.DescriptorTable.pDescriptorRanges[i];
				ss << "    Type=";
				switch (descRange.RangeType)
				{
				case D3D12_DESCRIPTOR_RANGE_TYPE_SRV: ss << "SRV"; break;
				case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: ss << "UAV"; break;
				case D3D12_DESCRIPTOR_RANGE_TYPE_CBV: ss << "CBV"; break;
				case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER: ss << "Sampler"; break;
				}
				ss << ", Count=" << descRange.NumDescriptors;
				ss << ", Register=" << descRange.BaseShaderRegister;
				ss << ", Space=" << descRange.RegisterSpace;
				ss << ", Flags=" << descRange.Flags;
				ss << ", Offset=" << static_cast<int>(descRange.OffsetInDescriptorsFromTableStart);
				ss << std::endl;
			}
		}
		break;
		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
		{
			ss << "    ";
			ss << "Register=" << rootParam.Constants.ShaderRegister;
			ss << ", Space=" << rootParam.Constants.RegisterSpace;
			ss << ", Count=" << rootParam.Constants.Num32BitValues << "x4 bytes";
			ss << std::endl;
		}
		break;
		default:
		{
		}
		break;
		}
		index++;
	}
	for (const auto& sampler : vStaticSamplers)
	{
		ss << "StaticSampler Register=" << sampler.ShaderRegister;
		ss << ", Space=" << sampler.RegisterSpace;
		ss << ", Visibility=";
		PrintShaderVisibility(sampler.ShaderVisibility);
		ss << std::endl;
	}
	ss << "TotalSize=" << totalSize << " bytes (256 max)";
	ss << ", Flags=";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
		ss << "[AIAIL]";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS)
		ss << "[DVSRA]";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS)
		ss << "[DHSRA]";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS)
		ss << "[DDSRA]";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS)
		ss << "[DGSRA]";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS)
		ss << "[DPSRA]";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT)
		ss << "[ASO]";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE)
		ss << "[LRS]";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS)
		ss << "[DASRA]";
	if (rootSignatureFlags & D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS)
		ss << "[DMSRA]";

	_debugInfo = ss.str();
}

void ShaderD3D12::UpdateUtilization() const
{
	VERUS_QREF_RENDERER;

	int setNumber = 0;
	for (const auto& dsd : _vDescriptorSetDesc)
	{
		StringStream ss;
		ss << "set=" << setNumber << ", " << _sourceName;
		renderer.AddUtilization(_C(ss.str()), dsd._index, dsd._capacity);
		setNumber++;
	}
}
