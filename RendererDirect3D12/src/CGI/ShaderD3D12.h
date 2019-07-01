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
			VERUS_TYPEDEFS(Compiled);
			typedef Map<String, Compiled> TMapCompiled;

			struct UniformBuffer
			{
				ComPtr<ID3D12Resource> _pConstantBuffer;
				BYTE*                  _pMappedData = nullptr;
				const void*            _pSrc = nullptr;
				int                    _size = 0;
				int                    _sizeAligned = 0;
				int                    _capacity = 1;
				int                    _capacityInBytes = 0;
				int                    _offset = 0;
				ShaderStageFlags       _stageFlags = ShaderStageFlags::vs_fs;
			};

			TMapCompiled                _mapCompiled;
			Vector<UniformBuffer>       _vUniformBuffers;
			ComPtr<ID3D12RootSignature> _pRootSignature;
			UINT64                      _currentFrame = UINT64_MAX;

		public:
			ShaderD3D12();
			virtual ~ShaderD3D12() override;

			virtual void Init(CSZ source, CSZ sourceName, CSZ* branches) override;
			virtual void Done() override;

			virtual void BindUniformBufferSource(int descSet, const void* pSrc, int size, int capacity, ShaderStageFlags stageFlags) override;
			virtual void CreatePipelineLayout() override;

			virtual void BeginBindDescriptors() override;
			virtual void EndBindDescriptors() override;

			//
			// D3D12
			//

			ID3DBlob* GetD3DBlob(CSZ branch, Stage stage) const { return _mapCompiled.at(branch)._pBlobs[+stage].Get(); }
			int GetNumStages(CSZ branch) const { return _mapCompiled.at(branch)._numStages; }

			ID3D12RootSignature* GetD3DRootSignature() const { return _pRootSignature.Get(); }

			UINT ToRootParameterIndex(int descSet) const;
			bool TryRootConstants(int descSet, RBaseCommandBuffer cb);
			CD3DX12_GPU_DESCRIPTOR_HANDLE UpdateUniformBuffer(int descSet);

			void OnError(CSZ s);
		};
		VERUS_TYPEDEFS(ShaderD3D12);
	}
}
