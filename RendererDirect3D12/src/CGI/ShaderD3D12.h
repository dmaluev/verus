// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		struct ShaderInclude : public ID3DInclude
		{
		public:
			virtual HRESULT STDMETHODCALLTYPE Open(
				D3D_INCLUDE_TYPE IncludeType,
				LPCSTR pFileName,
				LPCVOID pParentData,
				LPCVOID* ppData,
				UINT* pBytes) override;
			virtual HRESULT STDMETHODCALLTYPE Close(LPCVOID pData) override;
		};
		VERUS_TYPEDEFS(ShaderInclude);

		class ShaderD3D12 : public BaseShader
		{
		public:
			enum SET_MOD
			{
				SET_MOD_TO_VIEW_HEAP = 0x10000
			};

			struct Compiled
			{
				ComPtr<ID3DBlob> _pBlobs[+Stage::count];
				String           _entry;
				int              _stageCount = 0;
			};
			VERUS_TYPEDEFS(Compiled);

		private:
			typedef Map<String, Compiled> TMapCompiled;

			struct DescriptorSetDesc
			{
				Vector<Sampler>        _vSamplers;
				ComPtr<ID3D12Resource> _pConstantBuffer;
				D3D12MA::Allocation* _pMaAllocation = nullptr;
				DescriptorHeap         _dhDynamicOffsets;
				BYTE* _pMappedData = nullptr;
				const void* _pSrc = nullptr;
				int                    _size = 0;
				int                    _alignedSize = 0;
				int                    _capacity = 1;
				int                    _capacityInBytes = 0;
				int                    _offset = 0;
				int                    _peakLoad = 0;
				int                    _index = 0;
				ShaderStageFlags       _stageFlags = ShaderStageFlags::vs_fs;
				bool                   _staticSamplersOnly = true;
			};

			struct ComplexSet
			{
				Vector<TexturePtr> _vTextures;
				DescriptorHeap     _dhViews;
				DescriptorHeap     _dhSamplers;
				HandlePair         _hpBase;
			};
			VERUS_TYPEDEFS(ComplexSet);

			TMapCompiled                _mapCompiled;
			Vector<DescriptorSetDesc>   _vDescriptorSetDesc;
			Vector<ComplexSet>          _vComplexSets;
			ComPtr<ID3D12RootSignature> _pRootSignature;
			String                      _debugInfo;
			UINT64                      _currentFrame = UINT64_MAX;
			bool                        _compute = false;

		public:
			ShaderD3D12();
			virtual ~ShaderD3D12() override;

			virtual void Init(CSZ source, CSZ sourceName, CSZ* branches) override;
			virtual void Done() override;

			virtual void CreateDescriptorSet(int setNumber, const void* pSrc, int size, int capacity, std::initializer_list<Sampler> il, ShaderStageFlags stageFlags) override;
			virtual void CreatePipelineLayout() override;
			virtual CSHandle BindDescriptorSetTextures(int setNumber, std::initializer_list<TexturePtr> il, const int* pMipLevels, const int* pArrayLayers) override;
			virtual void FreeDescriptorSet(CSHandle& complexSetHandle) override;

			virtual void BeginBindDescriptors() override;
			virtual void EndBindDescriptors() override;

			//
			// D3D12
			//

			RcCompiled GetCompiled(CSZ branch) const { return _mapCompiled.at(branch); }

			ID3D12RootSignature* GetD3DRootSignature() const { return _pRootSignature.Get(); }

			UINT ToRootParameterIndex(int setNumber) const;
			bool TryRootConstants(int setNumber, RBaseCommandBuffer cb);
			CD3DX12_GPU_DESCRIPTOR_HANDLE UpdateUniformBuffer(int setNumber, int complexSetHandle);
			CD3DX12_GPU_DESCRIPTOR_HANDLE UpdateSamplers(int setNumber, int complexSetHandle);
			int GetDescriptorSetCount() const { return static_cast<int>(_vDescriptorSetDesc.size()); }
			bool IsCompute() const { return _compute; }

			void OnError(CSZ s);

			void UpdateDebugInfo(
				const Vector<CD3DX12_ROOT_PARAMETER1>& vRootParams,
				const Vector<D3D12_STATIC_SAMPLER_DESC>& vStaticSamplers,
				D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags);

			void UpdateUtilization();
		};
		VERUS_TYPEDEFS(ShaderD3D12);
	}
}
