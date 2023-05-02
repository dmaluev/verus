// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// ShadowMapBaker:

ShadowMapBaker::ShadowMapBaker()
{
}

ShadowMapBaker::~ShadowMapBaker()
{
	Done();
}

void ShadowMapBaker::Init(int side)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_side = side;

	_rph = renderer->CreateShadowRenderPass(CGI::Format::unormD24uintS8);

	CGI::TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._name = "ShadowMapBaker.Tex";
	texDesc._format = CGI::Format::unormD24uintS8;
	texDesc._width = _side;
	texDesc._height = _side;
	texDesc._flags = CGI::TextureDesc::Flags::depthSampledR;
	_tex.Init(texDesc);

	_fbh = renderer->CreateFramebuffer(_rph,
		{
			_tex
		},
		_side,
		_side);
}

void ShadowMapBaker::Done()
{
	if (CGI::Renderer::IsLoaded())
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteFramebuffer(_fbh);
		renderer->DeleteRenderPass(_rph);
	}
	VERUS_DONE(ShadowMapBaker);
}

void ShadowMapBaker::Begin(RcVector3 dirToSun, RcVector3 up)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(96, 96, 96, 255), "ShadowMapBaker");

	Point3 eye, at;
	const float texSizeInMeters = 100;
	const float depth = texSizeInMeters * 10;

	at = wm.GetHeadCamera()->GetEyePosition() + wm.GetHeadCamera()->GetFrontDirection() * (texSizeInMeters * (1 / 3.f));

	if (_snapToTexels)
	{
		const Matrix4 matToLightSpace = Matrix4::lookAt(Point3(0), Point3(-dirToSun), up);
		const Matrix4 matFromLightSpace = VMath::orthoInverse(matToLightSpace);

		Point3 snappedPos = (matToLightSpace * at).getXYZ();
		const float texelSizeInMeters = texSizeInMeters / _side;
		snappedPos.setX(snappedPos.getX() - fmod(snappedPos.getX() + texelSizeInMeters * 0.5f, texelSizeInMeters));
		snappedPos.setY(snappedPos.getY() - fmod(snappedPos.getY() + texelSizeInMeters * 0.5f, texelSizeInMeters));
		at = (matFromLightSpace * snappedPos).getXYZ();
	}

	eye = at + dirToSun * (depth * (2 / 3.f));
	const float zNear = 1;
	const float zFar = zNear + depth;
	_passCamera.MoveEyeTo(eye);
	_passCamera.MoveAtTo(at);
	_passCamera.SetUpDirection(up);
	_passCamera.SetYFov(0);
	_passCamera.SetZNear(zNear);
	_passCamera.SetZFar(zFar);
	_passCamera.SetXMag(texSizeInMeters);
	_passCamera.SetYMag(texSizeInMeters);
	_passCamera.Update();
	_pPrevPassCamera = wm.SetPassCamera(&_passCamera);

	_config._unormDepthScale = depth;

	cb->BeginRenderPass(_rph, _fbh, { _tex->GetClearValue() },
		CGI::ViewportScissorFlags::setAllForFramebuffer);

	VERUS_RT_ASSERT(!_baking);
	_baking = true;
}

void ShadowMapBaker::End()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	auto cb = renderer.GetCommandBuffer();

	cb->EndRenderPass();

	VERUS_PROFILER_END_EVENT(cb);

	const Matrix4 toUV = Math::ToUVMatrix();
	_matShadow = toUV * _passCamera.GetMatrixVP();

	_pPrevPassCamera = wm.SetPassCamera(_pPrevPassCamera);
	VERUS_RT_ASSERT(&_passCamera == _pPrevPassCamera); // Check camera's integrity.
	_pPrevPassCamera = nullptr;

	VERUS_RT_ASSERT(_baking);
	_baking = false;
}

void ShadowMapBaker::UpdateMatrixForCurrentView()
{
	VERUS_QREF_WM;
	_matShadowForDS = _matShadow * wm.GetViewCamera()->GetMatrixInvV();
}

RcMatrix4 ShadowMapBaker::GetShadowMatrix() const
{
	return _matShadow;
}

RcMatrix4 ShadowMapBaker::GetShadowMatrixForDS() const
{
	return _matShadowForDS;
}

CGI::TexturePtr ShadowMapBaker::GetTexture() const
{
	if (!IsInitialized())
		return nullptr;
	return _tex;
}

// CascadedShadowMapBaker::

CascadedShadowMapBaker::CascadedShadowMapBaker()
{
	VERUS_FOR(i, 4)
	{
		_matShadowCSM[i] = Matrix4::identity();
		_matShadowCSM_DS[i] = Matrix4::identity();
		_matOffset[i] = Matrix4::identity();
	}
}

CascadedShadowMapBaker::~CascadedShadowMapBaker()
{
	Done();
}

void CascadedShadowMapBaker::Init(int side)
{
	VERUS_QREF_BLOOM;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (settings._sceneShadowQuality < App::Settings::Quality::high)
	{
		ShadowMapBaker::Init(side);
		renderer.GetDS().InitByCascadedShadowMapBaker(_tex);
		bloom.InitByCascadedShadowMapBaker(_tex);
		return;
	}

	VERUS_INIT();

	_side = side;
	_headCameraPreferredRange = 300;
	if (settings._sceneShadowQuality >= App::Settings::Quality::ultra)
	{
		_side *= 2;
		_headCameraPreferredRange = 1000;
	}

	_rph = renderer->CreateShadowRenderPass(CGI::Format::unormD24uintS8);

	CGI::TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._name = "ShadowMapBaker.Tex (CSM)";
	texDesc._format = CGI::Format::unormD24uintS8;
	texDesc._width = _side;
	texDesc._height = _side;
	texDesc._flags = CGI::TextureDesc::Flags::depthSampledR;
	_tex.Init(texDesc);

	_fbh = renderer->CreateFramebuffer(_rph,
		{
			_tex
		},
		_side,
		_side);

	const float x = 0.5f;
	const Vector3 s(x, x, 1);
	_matOffset[0] = VMath::appendScale(Matrix4::translation(Vector3(0, 0, 0)), s);
	_matOffset[1] = VMath::appendScale(Matrix4::translation(Vector3(x, 0, 0)), s);
	_matOffset[2] = VMath::appendScale(Matrix4::translation(Vector3(0, x, 0)), s);
	_matOffset[3] = VMath::appendScale(Matrix4::translation(Vector3(x, x, 0)), s);

	renderer.GetDS().InitByCascadedShadowMapBaker(_tex);
	bloom.InitByCascadedShadowMapBaker(_tex);
}

void CascadedShadowMapBaker::Done()
{
}

void CascadedShadowMapBaker::Begin(RcVector3 dirToSun, int slice, RcVector3 up)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::Begin(dirToSun);

	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	_currentSlice = slice;

	auto cb = renderer.GetCommandBuffer();

	switch (_currentSlice)
	{
	case 0:  VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(48, 48, 48, 255), "CascadedShadowMapBaker/0"); break;
	case 1:  VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(64, 64, 64, 255), "CascadedShadowMapBaker/1"); break;
	case 2:  VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(80, 80, 80, 255), "CascadedShadowMapBaker/2"); break;
	case 3:  VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(96, 96, 96, 255), "CascadedShadowMapBaker/3"); break;
	default: VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(96, 96, 96, 255), "CascadedShadowMapBaker"); break;
	}

	RcCamera headCamera = *wm.GetHeadCamera();
	_passCamera = headCamera;

	Point3 eye, at;
	float texWidthInMeters, texHeightInMeters, texSizeInMeters;
	float zNear, zFar;

	const Matrix4 matToLightSpace = Matrix4::lookAt(Point3(0), Point3(-dirToSun), up);
	const float headCameraRange = Math::Min<float>(_headCameraPreferredRange, headCamera.GetZFar() - headCamera.GetZNear());

	auto ComputeTextureSize = [&texWidthInMeters, &texHeightInMeters, &texSizeInMeters](Math::RcBounds bounds)
	{
		// Stabilize size:
		const Vector3 dims = bounds.GetDimensions();
		texWidthInMeters = glm::ceil(1.5f + dims.getX());
		texHeightInMeters = glm::ceil(1.5f + dims.getY());
		texSizeInMeters = Math::Max(texWidthInMeters, texHeightInMeters);
	};

	auto PrepareCameraArguments = [this, &eye, &at, &zNear, &zFar, &dirToSun, &matToLightSpace](RcPoint3 centerPosLS, float atOffset)
	{
		at = (VMath::orthoInverse(matToLightSpace) * centerPosLS).getXYZ() + dirToSun * atOffset;
		eye = at + dirToSun * _depth;
		zNear = 1;
		zFar = zNear + _depth;
	};

	if (0 == _currentSlice)
	{
		_matScreenVP = _passCamera.GetMatrixVP();
		_matScreenP = _passCamera.GetMatrixP();

		_depth = Math::Min(headCameraRange * 2, 2000.f);
	}

	if (settings._sceneShadowQuality >= App::Settings::Quality::ultra) // Adjusted for 7x7 PCF.
	{
		// Compute slice bounds (weights are 2, 4.5, 12, 39):
		_sliceBounds = Vector4(
			headCamera.GetZNear() + headCameraRange * (18.5f / 57.5f),
			headCamera.GetZNear() + headCameraRange * (6.5f / 57.5f),
			headCamera.GetZNear() + headCameraRange * (2 / 57.5f),
			headCamera.GetZNear());
	}
	else // Adjusted for 5x5 PCF.
	{
		// Compute slice bounds (weights are 2, 3, 6, 15):
		_sliceBounds = Vector4(
			headCamera.GetZNear() + headCameraRange * (10 / 26.f),
			headCamera.GetZNear() + headCameraRange * (5 / 26.f),
			headCamera.GetZNear() + headCameraRange * (2 / 26.f),
			headCamera.GetZNear());
	}

	// Default world camera with current slice bounds:
	switch (_currentSlice)
	{
	case 0:
		_passCamera.SetZNear(_sliceBounds.getX());
		_passCamera.SetZFar(headCamera.GetZNear() + headCameraRange);
		break;
	case 1:
		_passCamera.SetZNear(_sliceBounds.getY());
		_passCamera.SetZFar(_sliceBounds.getX());
		break;
	case 2:
		_passCamera.SetZNear(_sliceBounds.getZ());
		_passCamera.SetZFar(_sliceBounds.getY());
		break;
	case 3:
		_passCamera.SetZNear(_sliceBounds.getW());
		_passCamera.SetZFar(_sliceBounds.getZ());
		break;
	}
	_sliceBounds.setW(headCamera.GetZNear() + headCameraRange);
	_passCamera.Update(); // Prepare frustum for measurements.

	Point3 focusedCenterPos; // With stable Z.
	const Math::Bounds frustumBoundsLS = _passCamera.GetFrustum().GetBounds(matToLightSpace, &focusedCenterPos);
	ComputeTextureSize(frustumBoundsLS);
	const float atOffset = frustumBoundsLS.GetMin().getZ() - focusedCenterPos.getZ();

	if (_snapToTexels)
	{
		// Stabilize offset:
		const Point3 focusPoint = VMath::lerp(0.5f, _passCamera.GetFrustum().GetCorner(8), _passCamera.GetFrustum().GetCorner(9));
		const Point3 snappedPos = (matToLightSpace * focusPoint).getXYZ();
		const float invSide = 2.f / _side;
		const float texelSizeInMeters = texSizeInMeters * invSide;
		focusedCenterPos.setX(focusedCenterPos.getX() - fmod(snappedPos.getX() + texelSizeInMeters * 0.5f, texelSizeInMeters));
		focusedCenterPos.setY(focusedCenterPos.getY() - fmod(snappedPos.getY() + texelSizeInMeters * 0.5f, texelSizeInMeters));
	}

	PrepareCameraArguments(focusedCenterPos, atOffset);
	_passCamera.MoveEyeTo(eye);
	_passCamera.MoveAtTo(at);
	_passCamera.SetUpDirection(up);
	_passCamera.SetYFov(0);
	_passCamera.SetZNear(zNear);
	_passCamera.SetZFar(zFar);
	_passCamera.SetXMag(texSizeInMeters);
	_passCamera.SetYMag(texSizeInMeters);
	_passCamera.Update();
	_pPrevPassCamera = wm.SetPassCamera(&_passCamera);

	_config._unormDepthScale = zFar - zNear;

	if (0 == _currentSlice)
	{
		cb->BeginRenderPass(_rph, _fbh, { _tex->GetClearValue() },
			CGI::ViewportScissorFlags::none);
	}

	const float s = static_cast<float>(_side / 2);
	Vector4 rc;
	switch (_currentSlice)
	{
	case 0: rc = Vector4(0, 0, s, s); break;
	case 1: rc = Vector4(s, 0, s, s); break;
	case 2: rc = Vector4(0, s, s, s); break;
	case 3: rc = Vector4(s, s, s, s); break;
	}
	cb->SetViewport({ rc });
	cb->SetScissor({ rc });

	VERUS_RT_ASSERT(!_baking);
	_baking = true;
}

void CascadedShadowMapBaker::End(int slice)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::End();

	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(_currentSlice == slice);

	auto cb = renderer.GetCommandBuffer();

	if (3 == _currentSlice)
		cb->EndRenderPass();

	VERUS_PROFILER_END_EVENT(cb);

	const Matrix4 toUV = Math::ToUVMatrix();
	_matShadowCSM[_currentSlice] = _matOffset[_currentSlice] * toUV * _passCamera.GetMatrixVP();

	_pPrevPassCamera = wm.SetPassCamera(_pPrevPassCamera);
	VERUS_RT_ASSERT(&_passCamera == _pPrevPassCamera); // Check camera's integrity.
	_pPrevPassCamera = nullptr;

	_currentSlice = -1;

	VERUS_RT_ASSERT(_baking);
	_baking = false;
}

void CascadedShadowMapBaker::UpdateMatrixForCurrentView()
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::UpdateMatrixForCurrentView();

	VERUS_QREF_WM;
	VERUS_FOR(i, 4)
		_matShadowCSM_DS[i] = _matShadowCSM[i] * wm.GetViewCamera()->GetMatrixInvV();
}

RcMatrix4 CascadedShadowMapBaker::GetShadowMatrix(int slice) const
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::GetShadowMatrix();
	return _matShadowCSM[slice];
}

RcMatrix4 CascadedShadowMapBaker::GetShadowMatrixForDS(int slice) const
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::GetShadowMatrixForDS();
	return _matShadowCSM_DS[slice];
}
