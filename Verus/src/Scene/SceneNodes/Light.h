// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class LightData
		{
		public:
			Vector4        _color = Vector4::Replicate(1);
			Vector3        _dir = Vector3(0, 0, 1);
			CGI::LightType _lightType = CGI::LightType::omni;
			float          _radius = 10;
			float          _coneIn = cos(Math::ToRadians(35)); // [0, coneOut-10] angle cosine.
			float          _coneOut = cos(Math::ToRadians(45)); // [10, 80] angle cosine.
		};
		VERUS_TYPEDEFS(LightData);

		// Light is a scene node with light's info.
		class Light : public SceneNode
		{
			LightData    _data;
			Anim::Shaker _shaker;
			float        _baseIntensity = 1;
			bool         _async_loadedMesh = false;

		public:
			struct Desc
			{
				pugi::xml_node _node;
				CSZ            _name = nullptr;
				CSZ            _urlIntShaker = nullptr;
				float          _intShakerScale = 0.25f;
				LightData      _data;
				bool           _dynamic = false;
			};
			VERUS_TYPEDEFS(Desc);

			Light();
			~Light();

			void Init(RcDesc desc);
			void Done();

			virtual void Update() override;

			void SetLightType(CGI::LightType type);
			CGI::LightType GetLightType() const;
			virtual void SetColor(RcVector4 color) override;
			virtual Vector4 GetColor() override;
			void SetIntensity(float i);
			float GetIntensity() const;
			void SetDirection(RcVector3 dir);
			RcVector3 GetDirection() const;
			void SetRadius(float r);
			float GetRadius() const;
			void SetCone(float coneIn = 0, float coneOut = 0);
			float GetConeIn() const;
			float GetConeOut() const;
			Vector4 GetInstData() const;

			void DirFromPoint(RcPoint3 point, float radiusScale = 0);
			void ConeFromPoint(RcPoint3 point, bool coneIn = false);

			Transform3 GetTransformNoScale() const;
			virtual void SetTransform(RcTransform3 mat) override;
			virtual void RestoreTransform(RcTransform3 tr, RcVector3 rot, RcVector3 scale) override;
			Vector3 ComputeScale();
			void ComputeTransform();

			virtual void UpdateBounds() override;

			// Serialization:
			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;
			virtual void SaveXML(pugi::xml_node node) override;
			virtual void LoadXML(pugi::xml_node node) override;
		};
		VERUS_TYPEDEFS(Light);

		class LightPtr : public Ptr<Light>
		{
		public:
			void Init(Light::RcDesc desc);
		};
		VERUS_TYPEDEFS(LightPtr);

		class LightPwn : public LightPtr
		{
		public:
			~LightPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(LightPwn);
	}
}
