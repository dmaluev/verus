// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Effects
	{
		enum class BillboardType : int
		{
			none,
			screenViewplaneOriented,
			screenViewpointOriented,
			worldViewplaneOriented,
			worldViewpointOriented,
			axial
		};

		class Particle
		{
		public:
			Point3  _position = Point3(0);
			Point3  _prevPosition = Point3(0);
			Vector3 _velocity = Vector3(0, 1, 0);
			Vector3 _axis = Vector3(0, 1, 0);
			Vector4 _tcOffset = Vector4(0);
			Vector4 _beginColor = Vector4::Replicate(1);
			Vector4 _endColor = Vector4::Replicate(1);
			void* _pUser = nullptr;
			float   _invTotalTime = 0;
			float   _timeLeft = -1;
			float   _beginAdditive = 0;
			float   _endAdditive = 0;
			float   _beginSize = 0;
			float   _endSize = 0;
			float   _beginSpin = 0;
			float   _endSpin = 0;
		};
		VERUS_TYPEDEFS(Particle);

		class Particles;
		class ParticlesDelegate
		{
		public:
			virtual void Particles_OnUpdate(Particles& particles, int index, RParticle particle,
				RVector3 up, RVector3 normal, RTransform3 matAim) = 0;
		};
		VERUS_TYPEDEFS(ParticlesDelegate);

		class Particles : public Object
		{
#include "../Shaders/Particles.inc.hlsl"

			enum PIPE
			{
				PIPE_MAIN,
				PIPE_BILLBOARDS,
				PIPE_COUNT
			};

			struct Vertex
			{
				glm::vec4 pos;
				glm::vec2 tc0;
				UINT32 color;
				float psize;
			};
			VERUS_TYPEDEFS(Vertex);

			static CGI::ShaderPwn                s_shader;
			static CGI::PipelinePwns<PIPE_COUNT> s_pipe;
			static UB_ParticlesVS                s_ubParticlesVS;
			static UB_ParticlesFS                s_ubParticlesFS;

			Vector4            _tilesetSize = Vector4::Replicate(1);
			Vector3            _lifeTimeRange = Vector3(0, 1, 1);
			Vector3            _speedRange = Vector3(0, 1, 1);
			Vector3            _beginAdditiveRange = Vector3(0);
			Vector3            _endAdditiveRange = Vector3(0);
			Vector4            _beginColorMin = Vector4(0);
			Vector4            _beginColorDelta = Vector4(0);
			Vector4            _endColorMin = Vector4(0);
			Vector4            _endColorDelta = Vector4(0);
			Vector3            _beginSizeRange = Vector3(0, 1, 1);
			Vector3            _endSizeRange = Vector3(0, 1, 1);
			Vector3            _beginSpinRange = Vector3(0, 1, 1);
			Vector3            _endSpinRange = Vector3(0, 1, 1);
			Vector3            _gravity = Vector3(0, -9.8f, 0);
			String             _url;
			Vector<Particle>   _vParticles;
			Vector<Vertex>     _vVB;
			Vector<UINT16>     _vIB;
			CGI::GeometryPwn   _geo;
			CGI::TexturePwn    _tex;
			CGI::CSHandle      _csh;
			PParticlesDelegate _pDelegate = nullptr;
			const float* _pUserZone = nullptr;
			BillboardType      _billboardType = BillboardType::none;
			int                _tilesetX = 0;
			int                _tilesetY = 0;
			int                _indexCount = 0;
			int                _capacity = 0;
			int                _addAt = 0;
			int                _drawCount = 0;
			float              _ratio = 1;
			float              _brightness = 1;
			float              _bounceStrength = 0.5f;
			float              _gravityStrength = 1;
			float              _windStrength = 0;
			float              _flowRemainder = 0;
			float              _particlesPerSecond = 0;
			float              _zone = 0;
			bool               _collide = false;
			bool               _decal = false;
			bool               _flowEnabled = true;

		public:
			Particles();
			~Particles();

			static void InitStatic();
			static void DoneStatic();

			void Init(CSZ url);
			void Done();

			void Update(); // Call this after adding particles!
			void Draw();

			Str GetUrl() const { return _C(_url); }
			int GetTilesetX() const { return _tilesetX; }
			int GetTilesetY() const { return _tilesetY; }
			int GetCapacity() const { return _capacity; }
			int GetDrawCount() const { return _drawCount; }

			PParticlesDelegate SetDelegate(PParticlesDelegate p) { return Utils::Swap(_pDelegate, p); }
			void SetUserZone(const float* pZone) { _pUserZone = pZone; }

			void GetLifeTime(float& mn, float& mx) const;
			void SetLifeTime(float mn, float mx);
			void GetSpeed(float& mn, float& mx) const;
			void SetSpeed(float mn, float mx);
			void GetBeginAdditive(float& mn, float& mx) const;
			void SetBeginAdditive(float mn, float mx);
			void GetBeginColor(RVector4 mn, RVector4 mx) const;
			void SetBeginColor(RcVector4 mn, RcVector4 mx);
			void GetEndColor(RVector4 mn, RVector4 mx) const;
			void SetEndColor(RcVector4 mn, RcVector4 mx);
			void GetEndAdditive(float& mn, float& mx) const;
			void SetEndAdditive(float mn, float mx);
			void GetBeginSize(float& mn, float& mx) const;
			void SetBeginSize(float mn, float mx);
			void GetEndSize(float& mn, float& mx) const;
			void SetEndSize(float mn, float mx);
			void GetBeginSpin(float& mn, float& mx) const;
			void SetBeginSpin(float mn, float mx);
			void GetEndSpin(float& mn, float& mx) const;
			void SetEndSpin(float mn, float mx);
			float GetBrightness() const { return _brightness; }
			void SetBrightness(float value) { _brightness = value; }
			float GetBounce() const { return _bounceStrength; }
			void SetBounce(float value) { _bounceStrength = value; }
			float GetGravity() const { return _gravityStrength; }
			void SetGravity(float value) { _gravityStrength = value; }
			float GetWind() const { return _windStrength; }
			void SetWind(float value) { _windStrength = value; }
			float GetParticlesPerSecond() const { return _particlesPerSecond; }
			void SetParticlesPerSecond(float value) { _particlesPerSecond = value; }
			float GetZone() const { return _zone; }
			void SetZone(float value) { _zone = value; }

			int Add(RcPoint3 pos, RcVector3 dir,
				float scale = 1, PcVector4 pUserColor = nullptr, void* pUser = nullptr);
			void AddFlow(RcPoint3 pos, RcVector3 dirOffset,
				float scale = 1, PcVector4 pUserColor = nullptr, float* pUserFlowRemainder = nullptr, float intensity = 1);
			bool IsFlowEnabled() const { return _flowEnabled; }
			void EnableFlow(bool b = true) { _flowEnabled = b; }

			bool TimeCorrectedVerletIntegration(RParticle particle, RPoint3 point, RVector3 normal);
			bool EulerIntegration(RParticle particle, RPoint3 point, RVector3 normal);

			void PushPointSprite(int index, RcVector4 color, float size, float additive);
			void PushBillboard(int index, RcVector4 color, RcTransform3 matW, float additive);
			Transform3 GetBillboardMatrix(int index, float size, float spin, RcVector3 up, RcVector3 normal, RTransform3 matAim);
		};
		VERUS_TYPEDEFS(Particles);
	}
}
