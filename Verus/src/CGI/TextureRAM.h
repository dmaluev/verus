#pragma once

namespace verus
{
	namespace CGI
	{
		class TextureRAM : public BaseTexture
		{
			Vector<BYTE> _vBuffer;

		public:
			TextureRAM();
			virtual ~TextureRAM() override;

			virtual void Init(RcTextureDesc desc) override;
			virtual void Done() override;

			virtual void UpdateSubresource(const void* p, int mipLevel, int arrayLayer, PBaseCommandBuffer pCB) override;
			virtual bool ReadbackSubresource(void* p, PBaseCommandBuffer pCB) override;

			virtual void GenerateMips(PBaseCommandBuffer pCB) override;

			const BYTE* GetData() const { VERUS_RT_ASSERT(IsLoaded()); return _vBuffer.data(); }
		};
		VERUS_TYPEDEFS(TextureRAM);
	}
}
