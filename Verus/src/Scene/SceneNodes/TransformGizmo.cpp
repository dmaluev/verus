#include "verus.h"

using namespace verus;
using namespace verus::Scene;

TransformGizmo::TransformGizmo()
{
}

TransformGizmo::~TransformGizmo()
{
	Done();
}

void TransformGizmo::Init()
{
	VERUS_INIT();
}

void TransformGizmo::Done()
{
	VERUS_DONE(TransformGizmo);
}

void TransformGizmo::Draw()
{
	VERUS_QREF_HELPERS;
}

void TransformGizmo::DragController_GetParams(float& x, float& y)
{
	x = y = 0;
}

void TransformGizmo::DragController_SetParams(float x, float y)
{
}

void TransformGizmo::DragController_GetRatio(float& x, float& y)
{
	x = y = 0.001f;
}

void TransformGizmo::DragController_Begin()
{
}

void TransformGizmo::DragController_End()
{
}

void TransformGizmo::DragController_SetScale(float s)
{
}
