#pragma once

namespace verus
{
	namespace Scene
	{
		class TransformGizmo : public Object, public SceneNode, public Input::DragControllerDelegate
		{
		public:
			TransformGizmo();
			~TransformGizmo();

			void Init();
			void Done();

			void Draw();

			virtual void DragController_GetParams(float& x, float& y) override;
			virtual void DragController_SetParams(float x, float y) override;
			virtual void DragController_GetRatio(float& x, float& y) override;
			virtual void DragController_Begin() override;
			virtual void DragController_End() override;
			virtual void DragController_SetScale(float s) override;
		};
		VERUS_TYPEDEFS(TransformGizmo);
	}
}
