#pragma once

namespace verus
{
	namespace Scene
	{
		class Atmosphere : public Singleton<Atmosphere>, public Object
		{
		public:
#include "../Shaders/Sky.inc.hlsl"

			enum PIPE
			{
				PIPE_SKY,
				PIPE_CLOUDS,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_SKY,
				TEX_STARS,
				TEX_SUN,
				TEX_MOON,
				TEX_CLOUDS,
				TEX_CLOUDS_NM,
				TEX_COUNT
			};

		private:
			struct Clouds
			{
				Vector4              _phaseA = Vector4(0);
				Vector4              _phaseB = Vector4(0);
				float                _speedPhaseA = 0.017f;
				float                _speedPhaseB = 0.003f;
				Anim::Damping<float> _cloudiness = 0.25f;
			};

			struct Fog
			{
				Vector3 _color = Vector3(0);
				Vector3 _colorInit = Vector3(0);
				float   _start = 10000;
				float   _end = 100000;
				float   _startEx = 0;
				float   _endEx = 0;
				float   _density = 0.0002f;
			};

			struct Sun
			{
				Matrix3 _matTilt;
				Vector3 _dirToMidnight = Vector3(0, -1, 0);
				Vector3 _dirTo = Vector3(0, 1, 0);
				Vector3 _color = Vector3(0);
			};

			static UB_PerFrame      s_ubPerFrame;
			static UB_PerMaterialFS s_ubPerMaterialFS;
			static UB_PerMeshVS     s_ubPerMeshVS;
			static UB_PerObject     s_ubPerObject;

			CGI::ShaderPwn                _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwns<TEX_COUNT>   _tex;
			Clouds                        _clouds;
			Fog                           _fog;
			Sun                           _sun;
			Vector3                       _ambientColor = Vector3(0);
			CascadedShadowMap             _shadowMap;
			Mesh                          _skyDome;
			CGI::TextureRAM               _texSky;
			float                         _time = 0.5f;
			float                         _timeSpeed = 1 / 300.f;
			float                         _hdrScale = 1000;
			CGI::CSHandle                 _cshSkyFS;
			bool                          _night = false;
			bool                          _async_loaded = false;

		public:
			Atmosphere();
			virtual ~Atmosphere();

			void Init();
			void Done();

			void UpdateSun(float time);
			void Update();
			void DrawSky();

			void GetColor(int level, float* pOut, float scale);

			// Time:
			float GetTime() const { return _time; }
			void SetTime(float x) { _time = x; }
			void SetTimeSpeed(float x) { _timeSpeed = x; }

			RcVector3 GetAmbientColor() const { return _ambientColor; }

			// Clouds:
			Anim::Damping<float>& GetCloudiness() { return _clouds._cloudiness; }
			void SetCloudiness(float x) { _clouds._cloudiness = x; _clouds._cloudiness.ForceTarget(); }

			// Fog:
			RcVector3 GetFogColor() const { return _fog._color; }
			float GetFogDensity() const { return _fog._density; }
			void SetFogDensity(float d) { _fog._density = d; }

			// Sun:
			RcVector3 GetDirToSun() const;
			RcVector3 GetSunColor() const;

			// Shadow:
			void BeginShadow(int split);
			void EndShadow(int split);
			RCascadedShadowMap GetShadowMap() { return _shadowMap; }

			RcPoint3 GetEyePosition(PVector3 pDirFront = nullptr);
		};
		VERUS_TYPEDEFS(Atmosphere);
	}
}
