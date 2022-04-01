// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class Texture : public AllocatorAware
		{
			UINT64              _date = 0;
			UINT64              _nextSafeFrame = 0;
			CGI::TexturePwn     _texTiny;
			CGI::TexturePwns<2> _texHuge;
			int                 _refCount = 0;
			int                 _currentHuge = 0;
			float               _requestedPart = 0;
			bool                _streamParts = false;
			bool                _loading = false;

		public:
			Texture();
			~Texture();

			void Init(CSZ url, bool streamParts = false, bool sync = false, CGI::PcSamplerDesc pSamplerDesc = nullptr);
			bool Done();

			void AddRef() { _refCount++; }

			void Update();
			void UpdateStreaming();

			CGI::TexturePtr GetTex() const;
			CGI::TexturePtr GetTinyTex() const;
			CGI::TexturePtr GetHugeTex() const;

			void ResetPart() { _requestedPart = FLT_MAX; }
			void IncludePart(float part) { _requestedPart = Math::Min(_requestedPart, part); }
			int GetPart() const;
		};
		VERUS_TYPEDEFS(Texture);

		class TexturePtr : public Ptr<Texture>
		{
		public:
			void Init(CSZ url, bool streamParts = false, bool sync = false, CGI::PcSamplerDesc pSamplerDesc = nullptr);
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

			INT64          _cshFreeFrame = -1;
			String         _name;
			IO::Dictionary _dict;
			glm::vec2      _anisoSpecDir = glm::vec2(0, 1);
			float          _detail = 0;
			float          _emission = 0;
			float          _nmContrast = -2;
			float          _roughDiffuse = 0;
			glm::vec4      _solidA = glm::vec4(1, 1, 1, 0);
			glm::vec4      _solidN = glm::vec4(0.5f, 0.5f, 1, 0);
			glm::vec4      _solidX = glm::vec4(1, 0.5f, 0.5f, 0);
			float          _sssHue = 0.5f;
			Pick           _sssPick;
			glm::vec4      _tc0ScaleBias = glm::vec4(1, 1, 0, 0);
			TexturePwn     _texA;
			TexturePwn     _texN;
			TexturePwn     _texX;
			glm::vec4      _userColor = glm::vec4(1, 1, 1, 1);
			Pick           _userPick;
			glm::vec2      _xAnisoSpecScaleBias = glm::vec2(1, 0);
			glm::vec2      _xMetallicScaleBias = glm::vec2(1, 0);
			glm::vec2      _xRoughnessScaleBias = glm::vec2(1, 0);
			glm::vec2      _xWrapDiffuseScaleBias = glm::vec2(1, 0);
			Blending       _blending = Blending::opaque;
			CGI::CSHandle  _csh;
			CGI::CSHandle  _cshTiny;
			CGI::CSHandle  _cshTemp;
			CGI::CSHandle  _cshSimple;
			int            _aPart = -1;
			int            _nPart = -1;
			int            _xPart = -1;
			int            _refCount = 0;

			Material();
			~Material();

			void Init(RcDesc desc);
			bool Done();

			void AddRef() { _refCount++; }

			bool operator<(const Material& that) const;

			void Update();

			bool IsLoaded() const;
			VERUS_P(void LoadTextures(bool streamParts));

			void SetName(CSZ name) { _name = name; }

			String ToString(bool cleanDict = false);
			void FromString(CSZ txt);

			// Mesh interaction:
			CGI::CSHandle GetComplexSetHandle() const;
			CGI::CSHandle GetComplexSetHandleSimple() const;
			void BindDescriptorSetTextures();
			bool UpdateMeshUniformBuffer(float motionBlur = 1, bool resolveDitheringMaskEnabled = true);
			bool UpdateMeshUniformBufferSimple();

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
			TexturePwn      _texDefaultA;
			TexturePwn      _texDefaultN;
			TexturePwn      _texDefaultX;
			TexturePwn      _texDetail;
			TexturePwn      _texDetailN;
			TexturePwn      _texStrass;
			CGI::TexturePwn _texDummyShadow;
			CGI::CSHandle   _cshDefault; // For missing, non-mandatory materials.
			CGI::CSHandle   _cshDefaultSimple;

		public:
			MaterialManager();
			~MaterialManager();

			void Init();
			void InitCmd();
			void Done();
			void DeleteAll();

			void Update();

			CGI::TexturePtr GetDefaultTexture() { return _texDefaultA->GetTex(); }
			CGI::TexturePtr GetDetailTexture() { return _texDetail->GetTex(); }
			CGI::TexturePtr GetDetailNTexture() { return _texDetailN->GetTex(); }
			CGI::TexturePtr GetStrassTexture() { return _texStrass->GetTex(); }

			CGI::CSHandle GetDefaultComplexSetHandle(bool simple = false) const { return simple ? _cshDefaultSimple : _cshDefault; }

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
