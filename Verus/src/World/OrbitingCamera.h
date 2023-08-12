// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	class OrbitingCamera : public MainCamera, public Input::DragControllerDelegate
	{
		Anim::Elastic<float, true> _pitch;
		Anim::Elastic<float, true> _yaw;
		Anim::Elastic<float>       _radius;
		bool                       _elastic = false;

	public:
		OrbitingCamera();
		virtual ~OrbitingCamera();

		virtual void Update() override;
		void UpdateUsingEyeAt();
		void UpdateElastic();

		virtual void DragController_GetParams(float& x, float& y) override;
		virtual void DragController_SetParams(float x, float y) override;
		virtual void DragController_GetRatio(float& x, float& y) override;

		float GetPitch() const { return _pitch; }
		float GetYaw() const { return _yaw; }
		float GetTargetPitch() const { return _pitch.GetTarget(); }
		float GetTargetYaw() const { return _yaw.GetTarget(); }
		void  SetPitch(float a);
		void  SetYaw(float a);

		float GetRadius() const { return _radius; }
		float GetTargetRadius() const { return _radius.GetTarget(); }
		void SetRadius(float r);
		void MulRadiusBy(float x);

		void EnableElastic(bool b = true) { _elastic = b; }
	};
	VERUS_TYPEDEFS(OrbitingCamera);
}
