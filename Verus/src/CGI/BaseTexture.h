#pragma once

namespace verus
{
	namespace CGI
	{
		struct SamplerDesc
		{
			Vector4 _borderColor = Vector4(0);
			char    _filterMagMinMip[4];
			char    _addressModeUVW[4];
			float   _mipLodBias = 0;
			float   _minLod = 0;
			float   _maxLod = 15;

			SamplerDesc()
			{
				VERUS_ZERO_MEM(_filterMagMinMip);
				VERUS_ZERO_MEM(_addressModeUVW);
			}

			void SetFilter(CSZ f) { strcpy(_filterMagMinMip, f); }
			void SetAddressMode(CSZ a) { strcpy(_addressModeUVW, a); }
			void Set(CSZ f, CSZ a) { SetFilter(f); SetAddressMode(a); }
		};
		VERUS_TYPEDEFS(SamplerDesc);

		struct TextureDesc
		{
			enum class Flags : UINT32
			{
				none = 0,
				colorAttachment = (1 << 0),
				inputAttachment = (1 << 1),
				depthSampled = (1 << 2),
				generateMips = (1 << 3),
				sync = (1 << 4)
			};

			Vector4       _clearValue = Vector4(0);
			PcSamplerDesc _pSamplerDesc = nullptr;
			CSZ           _url = nullptr;
			CSZ* _urls = nullptr;
			Format        _format = Format::unormR8G8B8A8;
			int           _width = 0;
			int           _height = 0;
			short         _depth = 1;
			short         _mipLevels = 1;
			short         _arrayLayers = 1;
			short         _sampleCount = 1;
			Flags         _flags = Flags::none;
			short         _texturePart = 0;

			TextureDesc(CSZ url = nullptr) : _url(url) {}
		};
		VERUS_TYPEDEFS(TextureDesc);

		class BaseTexture : public Object, public IO::AsyncCallback
		{
		protected:
			Vector4     _size = Vector4(0);
			String      _name;
			TextureDesc _desc;
			int         _part = 0;
			int         _bytesPerPixel = 0;

			BaseTexture();
			virtual ~BaseTexture();

		public:
			virtual void Init(RcTextureDesc desc) = 0;
			virtual void Done() = 0;

			bool IsLoaded() const { return _desc._width != 0; }
			Str GetName() const { return _C(_name); }

			RcTextureDesc GetDesc() const { return _desc; }
			RcVector4 GetClearValue() const { return _desc._clearValue; }
			Format GetFormat() const { return _desc._format; }
			int GetWidth() const { return _desc._width; }
			int GetHeight() const { return _desc._height; }
			int GetDepth() const { return _desc._depth; }
			int GetMipLevelCount() const { return _desc._mipLevels; }
			int GetArrayLayerCount() const { return _desc._arrayLayers; }

			int GetPart() const { return _part; }
			RcVector4 GetSize() const { return _size; }
			bool IsSRGB() const;

			void LoadDDS(CSZ url, int texturePart = 0);
			void LoadDDS(RcBlob blob);
			void LoadDDSArray(CSZ* urls);

			virtual void Async_Run(CSZ url, RcBlob blob) override;

			virtual void UpdateImage(int mipLevel, const void* p, int arrayLayer = 0, BaseCommandBuffer* pCB = nullptr) = 0;

			virtual void GenerateMips(BaseCommandBuffer* pCB = nullptr) = 0;

			static int FormatToBytesPerPixel(Format format);
			static bool IsBC(Format format);
			static bool Is4BitsBC(Format format);
			static bool IsDepthFormat(Format format);
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
