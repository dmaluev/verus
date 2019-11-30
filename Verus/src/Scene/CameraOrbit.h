#pragma once

namespace verus
{
	namespace Scene
	{
		class CameraOrbit : public MainCamera, public Input::DragControllerDelegate
		{
			float _pitch = 0;
			float _yaw = 0;
			float _radius = 10;

		public:
			virtual void Update() override;

			virtual void DragController_GetParams(float& x, float& y) override;
			virtual void DragController_SetParams(float x, float y) override;
			virtual void DragController_GetRatio(float& x, float& y) override;

			float GetRadius() const { return _radius; }
			void MulRadiusBy(float a);

			float	GetPitch()	const { return _pitch; }
			float	GetYaw()	const { return _yaw; }
			void	SetPitch(float a) { _pitch = a; }
			void	SetYaw(float a) { _yaw = a; }

			virtual void SaveState(int slot) override;
			virtual void LoadState(int slot) override;
		};
		VERUS_TYPEDEFS(CameraOrbit);
	}
}
