#pragma once

namespace verus
{
	namespace Scene
	{
		class Texture : public AllocatorAware
		{
			UINT64          _date = 0;
			CGI::TexturePwn _texTiny;
			CGI::TexturePwn _texHuge;
			int             _refCount = 0;
			float           _requestedPart = 0;
			bool            _streamParts = false;

		public:
			Texture();
			~Texture();

			void Init(CSZ url, bool streamParts = false, bool sync = false, CGI::PcSamplerDesc pSamplerDes = nullptr);
			bool Done();
			void AddRef() { _refCount++; }

			void Update();

			CGI::TexturePtr GetTex() const { return (_texHuge && _texHuge->IsLoaded()) ? _texHuge : _texTiny; }

			void ResetPart() { _requestedPart = FLT_MAX; }
			void IncludePart(float part) { _requestedPart = Math::Min(_requestedPart, part); }
		};
		VERUS_TYPEDEFS(Texture);

		class TexturePtr : public Ptr<Texture>
		{
		public:
			void Init(CSZ url, bool streamParts = false, bool sync = false, CGI::PcSamplerDesc pSamplerDes = nullptr);
		};
		VERUS_TYPEDEFS(TexturePtr);

		class TexturePwn : public TexturePtr
		{
		public:
			~TexturePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(TexturePwn);

		class Material : public Object
		{
		public:
			struct Desc
			{
				CSZ  _name = nullptr;
				bool _load = false;
				bool _streamParts = true;
				bool _mandatory = true;
			};
			VERUS_TYPEDEFS(Desc);

			enum class Blending : int
			{
				opaque,
				alphaTest,
				transparent,
				colorAdd,
				colorFilter
			};

			class Pick : public glm::vec4
			{
			public:
				Pick();
				Pick(const glm::vec4& v);
				void SetColor(float r, float g, float b);
				void SetAlpha(float x);
				void FromString(CSZ sz);
				String ToString();
			};

			class PickRound : public glm::vec4
			{
			public:
				PickRound();
				PickRound(const glm::vec4& v);
				void SetUV(float u, float v);
				void SetRadius(float r);
				void SetAlpha(float x);
				glm::vec4 ToPixels() const;
				void FromPixels(const glm::vec4& pix);
			};

			String         _name;
			IO::Dictionary _dict;
			glm::vec2      _alphaSwitch = glm::vec2(1, 1);
			glm::vec2      _anisoSpecDir = glm::vec2(0, 1);
			float          _detail = 0;
			float          _emission = 0;
			Pick           _emissionPick;
			PickRound      _eyePick;
			float          _gloss = 10;
			Pick           _glossPick;
			glm::vec2      _glossScaleBias = glm::vec2(1, 0);
			float          _hairDesat = 0;
			Pick           _hairPick;
			glm::vec2      _lamScaleBias = glm::vec2(1, 0);
			float          _lightPass = 1;
			Pick           _metalPick;
			Pick           _skinPick;
			glm::vec2      _specScaleBias = glm::vec2(1, 0);
			glm::vec4      _tc0ScaleBias = glm::vec4(1, 1, 0, 0);
			TexturePwn     _texAlbedo;
			glm::vec4      _texEnableAlbedo = glm::vec4(0.5f, 0.5f, 0.5f, 1);
			TexturePwn     _texNormal;
			glm::vec4      _texEnableNormal = glm::vec4(0.5f, 0.5f, 1.0f, 1);
			glm::vec4      _userColor = glm::vec4(0, 0, 0, 0);
			Pick           _userPick;
			Blending       _blending = Blending::opaque;
			CGI::CSHandle  _csh;
			int            _refCount = 0;

			Material();
			~Material();

			void Init(RcDesc desc);
			bool Done();

			void AddRef() { _refCount++; }

			bool operator<(const Material& that) const;

			void Update();

			bool IsLoaded() const;
			bool IsMissing() const;
			VERUS_P(void LoadTextures(bool streamParts));

			void SetName(CSZ name) { _name = name; }

			String ToString();
			void FromString(CSZ txt);

			// Mesh interaction:
			CGI::CSHandle GetComplexSetHandle() const { return _csh; }
			void BindDescriptorSetTextures();
			bool UpdateMeshUniformBuffer(float motionBlur = 1);

			void IncludePart(float part);
		};
		VERUS_TYPEDEFS(Material);

		class MaterialPtr : public Ptr<Material>
		{
		public:
			void Init(Material::RcDesc desc);
		};
		VERUS_TYPEDEFS(MaterialPtr);

		class MaterialPwn : public MaterialPtr
		{
		public:
			~MaterialPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(MaterialPwn);

		typedef StoreUnique<String, Texture> TStoreTextures;
		typedef StoreUnique<String, Material> TStoreMaterials;
		class MaterialManager : public Singleton<MaterialManager>, public Object, private TStoreTextures, private TStoreMaterials
		{
			TexturePwn    _texDefaultAlbedo;
			TexturePwn    _texDefaultNormal;
			TexturePwn    _texDetail;
			TexturePwn    _texStrass;
			CGI::CSHandle _cshDefault; // For missing, non-mandatory materials.

		public:
			MaterialManager();
			~MaterialManager();

			void Init();
			void InitCmd();
			void Done();
			void DeleteAll();

			void Update();

			CGI::TexturePtr GetDefaultTexture() { return _texDefaultAlbedo->GetTex(); }
			CGI::TexturePtr GetDetailTexture() { return _texDetail->GetTex(); }
			CGI::TexturePtr GetStrassTexture() { return _texStrass->GetTex(); }

			CGI::CSHandle GetDefaultComplexSetHandle() const { return _cshDefault; }

			// Textures:
			PTexture InsertTexture(CSZ url);
			PTexture FindTexture(CSZ url);
			void     DeleteTexture(CSZ url);
			void     DeleteAllTextures();

			// Materials:
			PMaterial InsertMaterial(CSZ name);
			PMaterial FindMaterial(CSZ name);
			void      DeleteMaterial(CSZ name);
			void      DeleteAllMaterials();

			void Serialize(IO::RSeekableStream stream);
			void Deserialize(IO::RStream stream);

			Vector<String> GetTextureNames() const;
			Vector<String> GetMaterialNames() const;

			void ResetPart();
			static float ComputePart(float distSq, float objectRadius);
		};
		VERUS_TYPEDEFS(MaterialManager);
	}
}
