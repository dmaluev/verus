// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class CameraOrbit : public MainCamera, public Input::DragControllerDelegate
		{
			Anim::Elastic<float, true> _pitch;
			Anim::Elastic<float, true> _yaw;
			Anim::Elastic<float>       _radius;
			bool                       _elastic = false;

		public:
			CameraOrbit();
			virtual ~CameraOrbit();

			virtual void Update() override;
			void UpdateUsingEyeAt();
			void UpdateElastic();

			virtual void DragController_GetParams(float& x, float& y) override;
			virtual void DragController_SetParams(float x, float y) override;
			virtual void DragController_GetRatio(float& x, float& y) override;

			float GetRadius() const { return _radius; }
			void SetRadius(float r) { _radius = r; _radius.ForceTarget(); }
			void MulRadiusBy(float a);

			float GetPitch() const { return _pitch; }
			float GetYaw() const { return _yaw; }
			void  SetPitch(float a) { _pitch = Math::WrapAngle(a); _pitch.ForceTarget(); }
			void  SetYaw(float a) { _yaw = Math::WrapAngle(a); _yaw.ForceTarget(); }

			void EnableElastic(bool b = true) { _elastic = b; }
		};
		VERUS_TYPEDEFS(CameraOrbit);
	}
}
