#pragma once

namespace verus
{
	namespace CGI
	{
		struct ShaderDesc
		{
			CSZ  _url = nullptr;
			CSZ  _source = nullptr;
			CSZ* _branches = nullptr;
			CSZ* _ignoreList = nullptr;
			bool _saveCompiled = false;

			ShaderDesc(CSZ url = nullptr) : _url(url) {}
		};
		VERUS_TYPEDEFS(ShaderDesc);

		class BaseShader : public Object
		{
			static CSZ s_branchCommentMarker;

		public:
			enum class Stage : int
			{
				vs, // Vertex shader
				hs, // Tessellation control shader (Hull shader)
				ds, // Tessellation evaluation shader (Domain shader)
				gs, // Geometry shader
				fs, // Fragment shader (Pixel shader)
				cs, // Compute Shader
				count
			};

		protected:
			CSZ* _ignoreList = nullptr;
			bool _saveCompiled = false;

			BaseShader() = default;
			virtual ~BaseShader() = default;

		public:
			virtual void Init(CSZ source, CSZ sourceName, CSZ* branches) = 0;
			virtual void Done() = 0;

			void Load(CSZ url);
			static String Parse(
				CSZ branchDesc,
				RString entryVS,
				RString entryHS,
				RString entryDS,
				RString entryGS,
				RString entryFS,
				RString entryCS,
				RString stages,
				Vector<String>& vMacroName,
				Vector<String>& vMacroValue,
				CSZ prefix);
			static void TestParse();

			virtual void CreateDescriptorSet(int setNumber, const void* pSrc, int size,
				int capacity = 1, std::initializer_list<Sampler> il = {}, ShaderStageFlags stageFlags = ShaderStageFlags::vs_fs) = 0;
			virtual void CreatePipelineLayout() = 0;
			virtual int BindDescriptorSetTextures(int setNumber, std::initializer_list<TexturePtr> il, const int* pMips = nullptr) = 0;
			virtual void FreeDescriptorSet(int complexSetID) = 0;

			virtual void BeginBindDescriptors() = 0;
			virtual void EndBindDescriptors() = 0;

			void SetIgnoreList(CSZ* list) { _ignoreList = list; }
			bool IsInIgnoreList(CSZ name) const;

			void SetSaveCompiled(bool b) { _saveCompiled = b; }
			static void SaveCompiled(CSZ code, CSZ filename);
		};
		VERUS_TYPEDEFS(BaseShader);

		class ShaderPtr : public Ptr<BaseShader>
		{
		public:
			void Init(RcShaderDesc desc);
		};
		VERUS_TYPEDEFS(ShaderPtr);

		class ShaderPwn : public ShaderPtr
		{
		public:
			~ShaderPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(ShaderPwn);

		template<int NUM>
		class ShaderPwns : public Pwns<ShaderPwn, NUM>
		{
		};
	}
}
