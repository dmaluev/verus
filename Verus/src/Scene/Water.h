#pragma once

namespace verus
{
	namespace Scene
	{
		class Water : public Singleton<Water>, public Object
		{
		public:
#include "../Shaders/Water.inc.hlsl"
#include "../Shaders/WaterGen.inc.hlsl"

			enum SHADER
			{
				SHADER_MAIN,
				SHADER_GEN,
				SHADER_COUNT
			};

			enum PIPE
			{
				PIPE_MAIN,
				PIPE_GEN_HEIGHTMAP,
				PIPE_GEN_NORMALS,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_FOAM,
				TEX_SOURCE_HEIGHTMAP,
				TEX_GEN_HEIGHTMAP,
				TEX_GEN_NORMALS,
				TEX_REFLECTION,
				TEX_REFLECTION_DEPTH,

				TEX_REFRACT,
				TEX_REFLECT,
				TEX_REFLECT_DEPTH,
				TEX_COUNT
			};

		private:
			static const int s_maxHarmonics = 4;

			struct Vertex
			{
				glm::vec4 _pos;
			};

			static UB_WaterVS        s_ubWaterVS;
			static UB_WaterFS        s_ubWaterFS;
			static UB_Gen            s_ubGen;
			static UB_GenHeightmapFS s_ubGenHeightmapFS;
			static UB_GenNormalsFS   s_ubGenNormalsFS;

			PTerrain                      _pTerrain = nullptr;
			Vector<Vertex>                _vSwapBuffer;
			CGI::GeometryPwn              _geo;
			CGI::ShaderPwns<SHADER_COUNT> _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwns<TEX_COUNT>   _tex;
			CGI::TexturePtr               _texLand;
			CGI::CSHandle                 _cshWaterVS;
			CGI::CSHandle                 _cshWaterFS;
			CGI::RPHandle                 _rphGenHeightmap;
			CGI::FBHandle                 _fbhGenHeightmap;
			CGI::CSHandle                 _cshGenHeightmap;
			CGI::RPHandle                 _rphGenNormals;
			CGI::FBHandle                 _fbhGenNormals;
			CGI::CSHandle                 _cshGenNormals;
			CGI::RPHandle                 _rphReflection;
			CGI::FBHandle                 _fbhReflection;
			const int                     _genSide = 1024;
			const int                     _sideReflect = 256;
			int                           _gridWidth = 128;
			int                           _gridHeight = 512;
			int                           _indexCount = 0;
			const float                   _patchSide = 64;
			float                         _phase = 0;
			float                         _phaseWave = 0;
			float                         _amplitudes[s_maxHarmonics];
			bool                          _reflectionMode = false;
			bool                          _renderToTexture = false;
			bool                          _geoReady = false;

		public:
			Water();
			~Water();

			void Init(RTerrain terrain);
			void Done();

			void Update();
			void Draw();

			void BeginReflection(CGI::PBaseCommandBuffer pCB = nullptr);
			void EndReflection(CGI::PBaseCommandBuffer pCB = nullptr);

			void GenerateTextures();
			VERUS_P(void GenerateHeightmapTexture());
			VERUS_P(void GenerateNormalsTexture());

			// Caustics are highlights on ocean floor.
			CGI::TexturePtr GetCausticsTexture() const;

			// Render reflection when above or normal view when below water level.
			bool IsReflectionMode() const { return _reflectionMode; }

			// Is rendering done to water's renderer target?
			bool IsRenderToTexture() const { return _renderToTexture; }

			VERUS_P(void CreateWaterPlane());

			static float PhillipsSpectrum(float k);
		};
		VERUS_TYPEDEFS(Water);
	}
}
