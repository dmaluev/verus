// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
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

	struct ShaderResources
	{
		UINT _srvCount = 0;
		UINT _uavCount = 0;
		UINT _srvStartSlot = 0;
		UINT _uavStartSlot = 0;
		ID3D11ShaderResourceView* _srvs[VERUS_MAX_FB_ATTACH];
		ID3D11UnorderedAccessView* _uavs[VERUS_MAX_FB_ATTACH];
		ID3D11SamplerState* _samplers[VERUS_MAX_FB_ATTACH];
	};
	VERUS_TYPEDEFS(ShaderResources);

	class ShaderD3D11 : public BaseShader
	{
	public:
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
			Vector<Sampler>      _vSamplers;
			ComPtr<ID3D11Buffer> _pConstantBuffer;
			const void* _pSrc = nullptr;
			int                  _size = 0;
			int                  _alignedSize = 0;
			int                  _capacity = 1;
			int                  _capacityInBytes = 0;
			UINT                 _srvCount = 0;
			UINT                 _uavCount = 0;
			UINT                 _srvStartSlot = 0;
			UINT                 _uavStartSlot = 0;
			ShaderStageFlags     _stageFlags = ShaderStageFlags::vs_fs;
		};

		struct ComplexSet
		{
			Vector<TexturePtr>                 _vTextures;
			Vector<ID3D11ShaderResourceView*>  _vSRVs;
			Vector<ID3D11UnorderedAccessView*> _vUAVs;
		};
		VERUS_TYPEDEFS(ComplexSet);

		TMapCompiled              _mapCompiled;
		Vector<DescriptorSetDesc> _vDescriptorSetDesc;
		Vector<ComplexSet>        _vComplexSets;
		bool                      _compute = false;

	public:
		ShaderD3D11();
		virtual ~ShaderD3D11() override;

		virtual void Init(CSZ source, CSZ sourceName, CSZ* branches) override;
		virtual void Done() override;

		virtual void CreateDescriptorSet(int setNumber, const void* pSrc, int size, int capacity, std::initializer_list<Sampler> il, ShaderStageFlags stageFlags) override;
		virtual void CreatePipelineLayout() override;
		virtual CSHandle BindDescriptorSetTextures(int setNumber, std::initializer_list<TexturePtr> il, const int* pMipLevels, const int* pArrayLayers) override;
		virtual void FreeDescriptorSet(CSHandle& complexSetHandle) override;

		virtual void BeginBindDescriptors() override;
		virtual void EndBindDescriptors() override;

		//
		// D3D11
		//

		RcCompiled GetCompiled(CSZ branch) const { return _mapCompiled.at(branch); }

		ID3D11Buffer* UpdateConstantBuffer(int setNumber) const;
		ShaderStageFlags GetShaderStageFlags(int setNumber) const;
		void GetShaderResources(int setNumber, int complexSetHandle, RShaderResources shaderResources)  const;
		void GetSamplers(int setNumber, int complexSetHandle, RShaderResources shaderResources)  const;
		int GetDescriptorSetCount() const { return static_cast<int>(_vDescriptorSetDesc.size()); }
		bool IsCompute() const { return _compute; }

		void OnError(CSZ s) const;
	};
	VERUS_TYPEDEFS(ShaderD3D11);
}
