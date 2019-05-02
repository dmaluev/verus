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
			};
			VERUS_TYPEDEFS(Compiled);
			typedef Map<String, Compiled> TMapCompiled;

			TMapCompiled _mapCompiled;

		public:
			ShaderD3D12();
			virtual ~ShaderD3D12() override;

			virtual void Init(CSZ source, CSZ* branches) override;
			virtual void Done() override;

			//
			// D3D12
			//

			const ComPtr<ID3DBlob>& GetID3DBlob(CSZ branch, Stage stage) const { return _mapCompiled.at(branch)._pBlobs[+stage]; }

			void OnError(CSZ s);
		};
		VERUS_TYPEDEFS(ShaderD3D12);
	}
}
