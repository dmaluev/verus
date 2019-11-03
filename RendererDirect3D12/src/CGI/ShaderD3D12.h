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
			struct Compiled
			{
				ComPtr<ID3DBlob> _pBlobs[+Stage::count];
				int              _numStages = 0;
			};
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
				int                    _sizeAligned = 0;
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
				DescriptorHeap     _dhSrvUav;
				DescriptorHeap     _dhSamplers;
			};
			VERUS_TYPEDEFS(ComplexSet);

			TMapCompiled                _mapCompiled;
			Vector<DescriptorSetDesc>   _vDescriptorSetDesc;
			Vector<ComplexSet>          _vComplexSets;
			ComPtr<ID3D12RootSignature> _pRootSignature;
			UINT64                      _currentFrame = UINT64_MAX;
			bool                        _compute = false;

		public:
			ShaderD3D12();
			virtual ~ShaderD3D12() override;

			virtual void Init(CSZ source, CSZ sourceName, CSZ* branches) override;
			virtual void Done() override;

			virtual void CreateDescriptorSet(int setNumber, const void* pSrc, int size, int capacity, std::initializer_list<Sampler> il, ShaderStageFlags stageFlags) override;
			virtual void CreatePipelineLayout() override;
			virtual int BindDescriptorSetTextures(int setNumber, std::initializer_list<TexturePtr> il, const int* pMips) override;

			virtual void BeginBindDescriptors() override;
			virtual void EndBindDescriptors() override;

			//
			// D3D12
			//

			ID3DBlob* GetD3DBlob(CSZ branch, Stage stage) const { return _mapCompiled.at(branch)._pBlobs[+stage].Get(); }
			int GetNumStages(CSZ branch) const { return _mapCompiled.at(branch)._numStages; }

			ID3D12RootSignature* GetD3DRootSignature() const { return _pRootSignature.Get(); }

			UINT ToRootParameterIndex(int setNumber) const;
			bool TryRootConstants(int setNumber, RBaseCommandBuffer cb);
			CD3DX12_GPU_DESCRIPTOR_HANDLE UpdateUniformBuffer(int setNumber, int complexSetID, bool copyDescOnly = false);
			CD3DX12_GPU_DESCRIPTOR_HANDLE UpdateSamplers(int setNumber, int complexSetID);
			int GetNumDescriptorSets() const { return static_cast<int>(_vDescriptorSetDesc.size()); }
			bool IsCompute() const { return _compute; }

			void OnError(CSZ s);
		};
		VERUS_TYPEDEFS(ShaderD3D12);
	}
}
