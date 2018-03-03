#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// Camera:

void Camera::UpdateInternal()
{
	if (_update & Update::v)
		UpdateView();

	if (_update & Update::p)
	{
		if (0 == _fovY)
			_matP = Matrix4::MakeOrtho(_w, _h, _zNear, _zFar);
		else
			_matP = Matrix4::MakePerspective(_fovY, _aspectRatio, _zNear, _zFar);
	}
}

void Camera::Update()
{
	UpdateInternal();

	if (+_update)
		UpdateVP();

	_update = Update::none;
}

void Camera::UpdateView()
{
	_dirFront = VMath::normalizeApprox(_posAt - _posEye);
#ifdef VERUS_COMPARE_MODE
	_dirFront = VMath::normalizeApprox(Vector3(glm::round(m_dirFront.GLM()*glm::vec3(4, 4, 4))));
	_posEye = Point3(int(_posEye.getX()), int(_posEye.getY()), int(_posEye.getZ()));
	_matV = Matrix4::lookAt(_posEye, _posEye + _dirFront, _dirUp);
#else
	_matV = Matrix4::lookAt(_posEye, _posAt, _dirUp);
#endif
	_matVi = VMath::orthoInverse(_matV);
}

void Camera::UpdateVP()
{
	_matVP = _matP * _matV;
	_frustum.FromMatrix(_matVP);
}

void Camera::UpdateFFP()
{
	//VERUS_QREF_RENDER;
	//render->SetTransform(CGL::TS_VIEW, _matV);
	//render->SetTransform(CGL::TS_PROJECTION, _matP);
}

Vector4 Camera::GetZNearFarEx() const
{
	//VERUS_QREF_RENDER;
	//if (CGL::RENDERER_OPENGL != render.GetRenderer())
	//	return Vector4(_zNear, _zFar, _zFar / (_zFar - _zNear), _zFar*_zNear / (_zNear - _zFar));
	//else
	return Vector4(_zNear, _zFar, (_zFar + _zNear) / (_zFar - _zNear), -2 * _zFar*_zNear / (_zFar - _zNear));
}

void Camera::SetFrustumNear(float zNear)
{
	_frustum.SetNearPlane(_posEye, _dirFront, zNear);
}

void Camera::SetFrustumFar(float zFar)
{
	_frustum.SetFarPlane(_posEye, _dirFront, zFar);
}

void Camera::SetFOVH(float x)
{
	SetFOV(2 * atan(tan(x*0.5f) / _aspectRatio));
}

void Camera::GetClippingSpacePlane(RVector4 plane) const
{
	const Matrix4 m = VMath::transpose(VMath::inverse(_matVP));
	plane = m * plane;
}

void Camera::ExcludeWaterLine(float h)
{
	const float y = _posEye.getY();
	if (y > -h && y < h)
	{
		const float t = h - y;
		const Vector3 offset(0, t, 0);
		_posEye = _posEye + offset;
		_posAt = _posAt + offset;
	}
}

void Camera::SaveState(int slot)
{
	StringStream ss;
	//ss << CUtils::I().GetWritablePath() << "/CameraState.xml";
	//Utils::CXml xml;
	//xml.SetFilename(_C(ss.str()));
	//xml.Load();
	//char text[16];
	//sprintf_s(text, "slot%d", slot);
	//xml.Set(text, _C(_posEye.ToString() + "|" + _posAt.ToString()));
}

void Camera::LoadState(int slot)
{
	StringStream ss;
	//ss << CUtils::I().GetWritablePath() << "/CameraState.xml";
	//Utils::CXml xml;
	//xml.SetFilename(_C(ss.str()));
	//xml.Load();
	//char text[16];
	//sprintf_s(text, "slot%d", slot);
	//CSZ v = xml.GetS(text);
	//if (v)
	//{
	//	CSZ p = strchr(v, '|');
	//	_posEye.FromString(v);
	//	_posAt.FromString(p + 1);
	//	_update |= Update::v;
	//	Update();
	//}
}

// MainCamera:

void MainCamera::operator=(RcMainCamera that)
{
	Camera::operator=(that);
	_currentFrame = -1;
}

void MainCamera::Update()
{
	UpdateInternal();

	if (+_update)
		UpdateVP();

	_update = Update::none;
}

void MainCamera::UpdateVP()
{
	//VERUS_QREF_RENDER;
	//if (m_currentFrame != render.GetNumFrames())
	{
		_matPrevVP = GetMatrixVP();
		//m_currentFrame = render.GetNumFrames();
	}
	Camera::UpdateVP();
}

void MainCamera::GetPickingRay(RPoint3 pos, RVector3 dir) const
{
	VERUS_RT_ASSERT(_pCpp);
	//VERUS_QREF_RENDER;

	int x, y;
	_pCpp->GetPos(x, y);
	//const Vector3 v(
	//	((float(x + x) / render.GetWindowWidth()) - 1) / GetMatrixP().getElem(0, 0),
	//	-((float(y + y) / render.GetWindowHeight()) - 1) / GetMatrixP().getElem(1, 1),
	//	-1);
	//dir = GetMatrixVi().getUpper3x3()*v;
	pos = GetMatrixVi().getTranslation();
}

float MainCamera::ComputeMotionBlur(RcPoint3 pos, RcPoint3 posPrev) const
{
	Vector4 screenPos = GetMatrixVP()*pos;
	Vector4 screenPosPrev = GetMatrixPrevVP()*posPrev;
	screenPos /= screenPos.getW();
	screenPos.setZ(0);
	screenPos.setW(0);
	screenPosPrev /= screenPosPrev.getW();
	screenPosPrev.setZ(0);
	screenPosPrev.setW(0);
	return VMath::length(screenPos - screenPosPrev) * 10;
}
