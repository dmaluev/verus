// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
		{
			_matP = Matrix4::MakeOrtho(_w, _h, _zNear, _zFar);
		}
		else
		{
			_matP = Matrix4::MakePerspective(_fovY, _aspectRatio, _zNear, _zFar);
			_fovScale = 0.5f / tan(_fovY * 0.5f);
		}
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
	_frontDir = VMath::normalizeApprox(_atPos - _eyePos);
#ifdef VERUS_COMPARE_MODE
	_frontDir = VMath::normalizeApprox(Vector3(glm::round(m_dirFront.GLM() * glm::vec3(4, 4, 4))));
	_eyePos = Point3(int(_eyePos.getX()), int(_eyePos.getY()), int(_eyePos.getZ()));
	_matV = Matrix4::lookAt(_eyePos, _eyePos + _frontDir, _upDir);
#else
	_matV = Matrix4::lookAt(_eyePos, _atPos, _upDir);
#endif
	_matVi = VMath::orthoInverse(_matV);
}

void Camera::UpdateVP()
{
	_matVP = _matP * _matV;
	_frustum.FromMatrix(_matVP);
}

void Camera::EnableReflectionMode()
{
	const Transform3 matV = _matV;

	_eyePos.setY(-_eyePos.getY());
	_atPos.setY(-_atPos.getY());

	UpdateView();
	UpdateVP();

	const Transform3 tr = Transform3::scale(Vector3(1, -1, 1));
	_matV = matV * tr;
	_matVi = VMath::orthoInverse(_matV);
	_matVP = _matP * _matV;
}

Vector4 Camera::GetZNearFarEx() const
{
	return Vector4(_zNear, _zFar, _zFar / (_zFar - _zNear), _zFar * _zNear / (_zNear - _zFar));
}

void Camera::SetFrustumNear(float zNear)
{
	_frustum.SetNearPlane(_eyePos, _frontDir, zNear);
}

void Camera::SetFrustumFar(float zFar)
{
	_frustum.SetFarPlane(_eyePos, _frontDir, zFar);
}

void Camera::SetFovX(float x)
{
	SetFovY(2 * atan(tan(x * 0.5f) / _aspectRatio));
}

void Camera::GetClippingSpacePlane(RVector4 plane) const
{
	const Matrix4 m = VMath::transpose(VMath::inverse(_matVP));
	plane = m * plane;
}

void Camera::ExcludeWaterLine(float h)
{
	const float y = _eyePos.getY();
	if (y > -h && y < h)
	{
		const float t = h - y;
		const Vector3 offset(0, t, 0);
		_eyePos = _eyePos + offset;
		_atPos = _atPos + offset;
	}
}

void Camera::SaveState(int slot)
{
	StringStream ss;
	ss << _C(Utils::I().GetWritablePath()) << "/CameraState.xml";
	IO::Xml xml;
	xml.SetFilename(_C(ss.str()));
	xml.Load();
	char text[16];
	sprintf_s(text, "slot%d", slot);
	xml.Set(text, _C(_eyePos.ToString() + "|" + _atPos.ToString()));
}

void Camera::LoadState(int slot)
{
	StringStream ss;
	ss << _C(Utils::I().GetWritablePath()) << "/CameraState.xml";
	IO::Xml xml;
	xml.SetFilename(_C(ss.str()));
	xml.Load();
	char text[16];
	sprintf_s(text, "slot%d", slot);
	CSZ v = xml.GetS(text);
	if (v)
	{
		CSZ p = strchr(v, '|');
		_eyePos.FromString(v);
		_atPos.FromString(p + 1);
		_update |= Update::v;
		Update();
	}
}

// MainCamera:

void MainCamera::operator=(RcMainCamera that)
{
	Camera::operator=(that);
	_currentFrame = UINT64_MAX;
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
	VERUS_QREF_RENDERER;
	if (_currentFrame != renderer.GetFrameCount())
	{
		_matPrevVP = GetMatrixVP();
		_currentFrame = renderer.GetFrameCount();
	}
	Camera::UpdateVP();
}

void MainCamera::GetPickingRay(RPoint3 pos, RVector3 dir) const
{
	VERUS_RT_ASSERT(_pCpp);
	VERUS_QREF_RENDERER;

	int x, y;
	_pCpp->GetPos(x, y);
	const float clipSpaceX = (float(x + x + 1) / renderer.GetSwapChainWidth()) - 1;
	const float clipSpaceY = 1 - (float(y + y + 1) / renderer.GetSwapChainHeight());
	const Vector3 v(
		clipSpaceX / GetMatrixP().getElem(0, 0),
		clipSpaceY / GetMatrixP().getElem(1, 1),
		-1);
	dir = GetMatrixVi().getUpper3x3() * v;
	pos = GetMatrixVi().getTranslation();
}

float MainCamera::ComputeMotionBlur(RcPoint3 pos, RcPoint3 posPrev) const
{
	Vector4 screenPos = GetMatrixVP() * pos;
	Vector4 screenPosPrev = GetMatrixPrevVP() * posPrev;
	screenPos /= screenPos.getW();
	screenPos.setZ(0);
	screenPos.setW(0);
	screenPosPrev /= screenPosPrev.getW();
	screenPosPrev.setZ(0);
	screenPosPrev.setW(0);
	return VMath::length(screenPos - screenPosPrev) * 10;
}
