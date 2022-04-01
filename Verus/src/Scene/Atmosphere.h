// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
				PIPE_SKY_REFLECTION,
				PIPE_CLOUDS,
				PIPE_CLOUDS_REFLECTION,
				PIPE_SUN,
				PIPE_MOON,
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

			struct Vertex
			{
				float _pos[3];
				short _tc[2];
			};

		private:
			struct Clouds
			{
				Vector4              _phaseA = Vector4(0);
				Vector4              _phaseB = Vector4(0);
				float                _speedPhaseA = 0.017f;
				float                _speedPhaseB = 0.003f;
				Anim::Elastic<float> _cloudiness = 0.25f;
			};

			struct Fog
			{
				Vector3 _color = Vector3(0);
				float   _density[2] = {}; // Actual and base.
			};

			struct Sun
			{
				Matrix3  _matTilt;
				Vector3  _dirTo = Vector3(0, 1, 0);
				Vector3  _color = Vector3(0);
				LightPwn _light;
				float    _alpha = 1;
				float    _latitude = VERUS_PI * 0.25f;
			};

			struct Wind
			{
				Matrix3 _matPlantBending = Matrix3::identity();
				Vector3 _baseVelocity = Vector3(4, 0, 0);
				Vector3 _velocity = Vector3(4, 0, 0);
				Vector3 _accel = Vector3(0);
				Vector3 _jerk = Vector3(0);
				Vector3 _direction = Vector3(0);
				float   _speed = 0;
			};

			static UB_PerFrame      s_ubPerFrame;
			static UB_PerMaterialFS s_ubPerMaterialFS;
			static UB_PerMeshVS     s_ubPerMeshVS;
			static UB_PerObject     s_ubPerObject;

			CGI::GeometryPwn              _geo;
			CGI::ShaderPwn                _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwns<TEX_COUNT>   _tex;
			Clouds                        _clouds;
			Fog                           _fog;
			Sun                           _sun;
			Wind                          _wind;
			Vector3                       _ambientColor = Vector3(0);
			Vector3                       _ambientColorY0 = Vector3(0);
			Vector3                       _ambientColorY1 = Vector3(0);
			CubeMapBaker                  _cubeMapBaker;
			CascadedShadowMapBaker        _shadowMapBaker;
			Mesh                          _skyDome;
			CGI::TextureRAM               _texSky;
			float                         _time = 0.5f;
			float                         _timeSpeed = 1 / 300.f;
			float                         _ambientMagnitude = 1;
			CGI::CSHandle                 _cshSkyFS;
			CGI::CSHandle                 _cshSunFS;
			CGI::CSHandle                 _cshMoonFS;
			bool                          _night = false;
			bool                          _async_loaded = false;

		public:
			struct Report
			{
				float _time;
				float _speed;
				float _cloudiness;
			};
			VERUS_TYPEDEFS(Report);

			Atmosphere();
			virtual ~Atmosphere();

			void Init();
			void Done();

			void UpdateSun(float time);
			void UpdateWind();
			void Update();
			void DrawSky(bool reflection = false);

			void SampleSkyColor(float time, int level, float* pOut);
			static float GetMagnitude(float time, float noon, float dusk, float midnight);

			// Time:
			float GetTime() const { return _time; }
			void SetTime(float x) { _time = x; }
			float GetTimeSpeed() const { return _timeSpeed; }
			void SetTimeSpeed(float x) { _timeSpeed = x; }

			float GetAmbientMagnitude() const { return _ambientMagnitude; }
			RcVector3 GetAmbientColor() const { return _ambientColor; }
			RcVector3 GetAmbientColorY0() const { return _ambientColorY0; }
			RcVector3 GetAmbientColorY1() const { return _ambientColorY1; }

			// Clouds:
			Anim::Elastic<float>& GetCloudiness() { return _clouds._cloudiness; }
			void SetCloudiness(float x) { _clouds._cloudiness = x; _clouds._cloudiness.ForceTarget(); }

			// Fog:
			RcVector3 GetFogColor() const { return _fog._color; }
			float GetFogDensity() const { return _fog._density[0]; }
			float GetBaseFogDensity() const { return _fog._density[1]; }
			void SetBaseFogDensity(float d) { _fog._density[1] = d; }

			// Sun:
			RcVector3 GetDirToSun() const;
			RcVector3 GetSunColor() const;
			float GetSunAlpha() const;
			void OnSceneResetInitSunLight();
			void SetLatitude(float lat);

			// Wind:
			RcMatrix3 GetPlantBendingMatrix() const;
			RcVector3 GetBaseWindVelocity() const;
			void SetBaseWindVelocity(RcVector3 v);
			RcVector3 GetWindVelocity() const;
			RcVector3 GetWindDirection() const;
			float GetWindSpeed() const;

			// CubeMap:
			RCubeMapBaker GetCubeMapBaker() { return _cubeMapBaker; }

			// ShadowMap:
			void BeginShadow(int split);
			void EndShadow(int split);
			RCascadedShadowMapBaker GetShadowMapBaker() { return _shadowMapBaker; }

			void CreateCelestialBodyMesh();

			// Net:
			void GetReport(RReport report);
			void SetReport(RcReport report);
		};
		VERUS_TYPEDEFS(Atmosphere);
	}
}
