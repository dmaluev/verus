// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
		if (0 == _yFov)
		{
			_matP = Matrix4::MakeOrtho(_xMag, _yMag, _zNear, _zFar);
		}
		else
		{
			if (_yFov > 0)
			{
				_matP = Matrix4::MakePerspective(_yFov, _aspectRatio, _zNear, _zFar);
			}
			else
			{
				_yFov = -_yFov;
				_matP = Matrix4::MakePerspective(_yFov, _aspectRatio, _zNear, _zFar, false);
			}
			_fovScale = 0.5f / tan(_yFov * 0.5f);
		}
		_matInvP = VMath::inverse(_matP);
	}
}

void Camera::Update()
{
	UpdateInternal();

	if (+_update)
		UpdateVP();

	_update = Update::none;
}

void Camera::UpdateUsingViewDesc(CGI::RcViewDesc viewDesc)
{
	// Per eye data from head-mounted display with desired FOV and Z range.

	_matV = viewDesc._matV;
	_matP = viewDesc._matP;
	_matInvV = VMath::orthoInverse(_matV);
	_matInvP = VMath::inverse(_matP);

	const Matrix3 matR(viewDesc._pose._orientation);
	_frontDir = matR.getCol2();
	_upDir = matR.getCol1();

	_eyePos = viewDesc._pose._position;
	_atPos = _eyePos + _frontDir;

	_yFov = VERUS_PI * 0.5f;
	_aspectRatio = 1;
	_zNear = viewDesc._zNear;
	_zFar = viewDesc._zFar;

	UpdateVP();
	_update = Update::none;
}

void Camera::UpdateUsingHeadPose(Math::RcPose pose)
{
	// Head data from head-mounted display without FOV.

	_matV = VMath::orthoInverse(Transform3(pose._orientation, Vector3(pose._position)));
	_matInvV = VMath::orthoInverse(_matV);

	const Matrix3 matR(pose._orientation);
	_frontDir = matR.getCol2();
	_upDir = matR.getCol1();

	_eyePos = pose._position;
	_atPos = _eyePos + _frontDir;

	Update();
}

void Camera::UpdateZNearFar()
{
	_matP.UpdateZNearFar(_zNear, _zFar);
	_matInvP = VMath::inverse(_matP);

	UpdateVP();
	_update = Update::none;
}

void Camera::UpdateView()
{
	_frontDir = VMath::normalizeApprox(_atPos - _eyePos);
	_matV = Matrix4::lookAt(_eyePos, _atPos, _upDir);
	_matInvV = VMath::orthoInverse(_matV);
}

void Camera::UpdateVP()
{
	_matVP = _matP * _matV;
	_frustum.FromMatrix(_matVP);
}

void Camera::SetFrustumNear(float zNear)
{
	_frustum.SetNearPlane(_eyePos, _frontDir, zNear);
}

void Camera::SetFrustumFar(float zFar)
{
	_frustum.SetFarPlane(_eyePos, _frontDir, zFar);
}

void Camera::SetXFov(float xFov)
{
	SetYFov(2 * atan(tan(xFov * 0.5f) / _aspectRatio));
}

Vector4 Camera::GetZNearFarEx() const
{
	return Vector4(_zNear, _zFar, _zFar / (_zFar - _zNear), _zFar * _zNear / (_zNear - _zFar));
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
	_matInvV = VMath::orthoInverse(_matV);
	_matVP = _matP * _matV;
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

void Camera::ApplyMotion(CSZ boneName, Anim::RMotion motion, float time)
{
	auto pBone = motion.FindBone(boneName);
	if (!pBone)
		return;

	Quat q;
	Vector3 euler, pos;
	int state = 0;
	pBone->ComputeRotationAt(time, euler, q);
	pBone->ComputePositionAt(time, pos);
	pBone->ComputeTriggerAt(time, state);
	Point3 at;
	Vector3 up;
	if (state & 0x2)
	{
		Vector3 scale;
		pBone->ComputeScaleAt(time, scale);
		at = scale;
		up = Vector3(0, 1, 0);
	}
	else
	{
		const Matrix3 matR(q);
		at = pos + matR.getCol2();
		up = matR.getCol1();
	}
	MoveEyeTo(pos);
	MoveAtTo(at);
	SetUpDirection(up);
	Update();
}

void Camera::BakeMotion(CSZ boneName, Anim::RMotion motion, int frame)
{
	auto pBone = motion.FindBone(boneName);
	if (!pBone)
		return;

	const glm::quat q = glm::quatLookAt(-_frontDir.GLM(), _upDir.GLM());
	pBone->InsertKeyframeRotation(frame, q);
	pBone->InsertKeyframePosition(frame, _eyePos);
}

void Camera::SaveState(int slot)
{
	StringStream ss;
	ss << _C(Utils::I().GetWritablePath()) << "CameraState.xml";
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
	ss << _C(Utils::I().GetWritablePath()) << "CameraState.xml";
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

void MainCamera::UpdateUsingViewDesc(CGI::RcViewDesc viewDesc)
{
	Camera::UpdateUsingViewDesc(viewDesc);
	_matPrevVP = GetMatrixVP();
}

void MainCamera::UpdateUsingHeadPose(Math::RcPose pose)
{
	Camera::UpdateUsingHeadPose(pose);
	_matPrevVP = GetMatrixVP();
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

	if (_cutMotionBlur)
	{
		_cutMotionBlur = false;
		_matPrevVP = GetMatrixVP();
	}
}

float MainCamera::ComputeMotionBlur(RcPoint3 pos, RcPoint3 posPrev) const
{
	Vector4 ndcPos = GetMatrixVP() * pos;
	Vector4 ndcPosPrev = GetMatrixPrevVP() * posPrev;
	ndcPos /= ndcPos.getW();
	ndcPos.setZ(0);
	ndcPos.setW(0);
	ndcPosPrev /= ndcPosPrev.getW();
	ndcPosPrev.setZ(0);
	ndcPosPrev.setW(0);
	return VMath::length(ndcPos - ndcPosPrev) * 10;
}

void MainCamera::GetPickingRay(RPoint3 pos, RVector3 dir) const
{
	VERUS_RT_ASSERT(_pCpp);
	VERUS_QREF_RENDERER;

	int x, y;
	_pCpp->GetPos(x, y);
	const float ndcX = (float(x + x + 1) / renderer.GetCurrentViewWidth()) - 1;
	const float ndcY = 1 - (float(y + y + 1) / renderer.GetCurrentViewHeight());
	const Vector3 v(
		ndcX / GetMatrixP().getElem(0, 0),
		ndcY / GetMatrixP().getElem(1, 1),
		-1);
	dir = GetMatrixInvV().getUpper3x3() * v;
	pos = GetMatrixInvV().getTranslation();
}
