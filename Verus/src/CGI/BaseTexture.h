#pragma once

namespace verus
{
	namespace CGI
	{
		struct TextureDesc
		{
		};
		VERUS_TYPEDEFS(TextureDesc);

		class BaseTexture : public Object
		{
		protected:
			BaseTexture() = default;
			virtual ~BaseTexture() = default;

		public:
			virtual void Init(RcTextureDesc desc) = 0;
			virtual void Done() = 0;
		};
		VERUS_TYPEDEFS(BaseTexture);

		class TexturePtr : public Ptr<BaseTexture>
		{
		public:
			TexturePtr() = default;
			TexturePtr(nullptr_t) : Ptr(nullptr) {}

			void Init(RcTextureDesc desc);

			static TexturePtr From(PBaseTexture p);
		};
		VERUS_TYPEDEFS(TexturePtr);

		class TexturePwn : public TexturePtr
		{
		public:
			~TexturePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(TexturePwn);

		template<int NUM>
		class TexturePwns : public Pwns<TexturePwn, NUM>
		{
		};
	}
}
