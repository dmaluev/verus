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
	VERUS_DONE(ShadowMapBaker);
}

void ShadowMapBaker::UpdateMatrixForCurrentView()
{
	VERUS_QREF_WM;
	_matShadowDS = _matShadow * wm.GetViewCamera()->GetMatrixInvV();
}

void ShadowMapBaker::Begin(RcVector3 dirToSun, RcVector3 up)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

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

	// Setup light space camera and use it (used for terrain draw, etc.):
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

	renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh, { _tex->GetClearValue() },
		CGI::ViewportScissorFlags::setAllForFramebuffer);

	VERUS_RT_ASSERT(!_baking);
	_baking = true;
}

void ShadowMapBaker::End()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	renderer.GetCommandBuffer()->EndRenderPass();

	const Matrix4 m = Math::ToUVMatrix();
	_matShadow = m * _passCamera.GetMatrixVP();

	_pPrevPassCamera = wm.SetPassCamera(_pPrevPassCamera);
	VERUS_RT_ASSERT(&_passCamera == _pPrevPassCamera); // Check camera's integrity.
	_pPrevPassCamera = nullptr;

	VERUS_RT_ASSERT(_baking);
	_baking = false;
}

RcMatrix4 ShadowMapBaker::GetShadowMatrix() const
{
	return _matShadow;
}

RcMatrix4 ShadowMapBaker::GetShadowMatrixDS() const
{
	return _matShadowDS;
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
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::Init(side);

	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_side = side;
	if (settings._sceneShadowQuality >= App::Settings::Quality::ultra)
		_side *= 2;

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
}

void CascadedShadowMapBaker::Done()
{
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

void CascadedShadowMapBaker::Begin(RcVector3 dirToSun, int split, RcVector3 up)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::Begin(dirToSun);

	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	_currentSplit = split;

	RcCamera headCamera = *wm.GetHeadCamera();

	Point3 eye, at;
	float texWidthInMeters, texHeightInMeters, texSizeInMeters;
	float zNear, zFar;

	auto ComputeTextureSize = [&texWidthInMeters, &texHeightInMeters, &texSizeInMeters](Math::RcBounds bounds)
	{
		// Stabilize size:
		const Vector3 dims = bounds.GetDimensions();
		texWidthInMeters = glm::ceil(1.5f + dims.getX());
		texHeightInMeters = glm::ceil(1.5f + dims.getY());
		texSizeInMeters = Math::Max(texWidthInMeters, texHeightInMeters);
	};

	const Matrix4 matToLightSpace = Matrix4::lookAt(Point3(0), Point3(-dirToSun), up);
	const float headCameraMaxRange = (settings._sceneShadowQuality >= App::Settings::Quality::ultra) ? 1000.f : 300.f;
	const float headCameraRange = Math::Min<float>(headCameraMaxRange, headCamera.GetZFar() - headCamera.GetZNear()); // Clip terrain, etc.
	_passCamera = headCamera;

	auto PrepareCameraArguments = [this, &eye, &at, &zNear, &zFar, &dirToSun, &matToLightSpace](RcPoint3 centerPosLS)
	{
		at = (VMath::orthoInverse(matToLightSpace) * centerPosLS).getXYZ();
		eye = at + dirToSun * (_depth * (2 / 3.f));
		zNear = 1;
		zFar = zNear + _depth;
	};

	if (0 == _currentSplit)
	{
		_matScreenVP = _passCamera.GetMatrixVP();
		_matScreenP = _passCamera.GetMatrixP();

		_depth = Math::Min(headCameraRange * 10, 10000.f); // 10 times larger than side.

		_passCamera.SetZFar(_passCamera.GetZNear() + headCameraRange);
		_passCamera.Update(); // Prepare frustum for measurements.

		Point3 focusedCenterPos; // With stable Z.
		const Math::Bounds frustumBoundsLS = _passCamera.GetFrustum().GetBounds(matToLightSpace, &focusedCenterPos);
		ComputeTextureSize(frustumBoundsLS);

		// Setup CSM light space camera for full range (used for terrain layout, etc.):
		PrepareCameraArguments(focusedCenterPos);
		_passCameraCSM.MoveEyeTo(eye);
		_passCameraCSM.MoveAtTo(at);
		_passCameraCSM.SetUpDirection(up);
		_passCameraCSM.SetYFov(0);
		_passCameraCSM.SetZNear(zNear);
		_passCameraCSM.SetZFar(zFar);
		_passCameraCSM.SetXMag(texSizeInMeters);
		_passCameraCSM.SetYMag(texSizeInMeters);
		_passCameraCSM.Update();
	}

	if (settings._sceneShadowQuality >= App::Settings::Quality::ultra) // Adjusted for 7x7 PCF.
	{
		// Compute split ranges (weights are 2, 4.5, 12, 39):
		_splitRanges = Vector4(
			headCamera.GetZNear() + headCameraRange * (18.5f / 57.5f),
			headCamera.GetZNear() + headCameraRange * (6.5f / 57.5f),
			headCamera.GetZNear() + headCameraRange * (2 / 57.5f),
			headCamera.GetZNear());
	}
	else // Adjusted for 5x5 PCF.
	{
		// Compute split ranges (weights are 2, 3, 6, 15):
		_splitRanges = Vector4(
			headCamera.GetZNear() + headCameraRange * (10 / 26.f),
			headCamera.GetZNear() + headCameraRange * (5 / 26.f),
			headCamera.GetZNear() + headCameraRange * (2 / 26.f),
			headCamera.GetZNear());
	}

	// Default scene camera with the current split range:
	switch (_currentSplit)
	{
	case 0:
		_passCamera.SetZNear(_splitRanges.getX());
		_passCamera.SetZFar(headCamera.GetZNear() + headCameraRange);
		break;
	case 1:
		_passCamera.SetZNear(_splitRanges.getY());
		_passCamera.SetZFar(_splitRanges.getX());
		break;
	case 2:
		_passCamera.SetZNear(_splitRanges.getZ());
		_passCamera.SetZFar(_splitRanges.getY());
		break;
	case 3:
		_passCamera.SetZNear(_splitRanges.getW());
		_passCamera.SetZFar(_splitRanges.getZ());
		break;
	}
	_splitRanges.setW(headCamera.GetZNear() + headCameraRange);
	_passCamera.Update(); // Prepare frustum for measurements.

	Point3 focusedCenterPos; // With stable Z.
	const Math::Bounds frustumBoundsLS = _passCamera.GetFrustum().GetBounds(matToLightSpace, &focusedCenterPos);
	ComputeTextureSize(frustumBoundsLS);

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

	// Setup light space camera and use it (used for terrain draw, etc.):
	PrepareCameraArguments(focusedCenterPos);
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

	if (0 == _currentSplit)
	{
		renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh, { _tex->GetClearValue() },
			CGI::ViewportScissorFlags::setAllForFramebuffer);
	}

	const float s = static_cast<float>(_side / 2);
	switch (_currentSplit)
	{
	case 0: renderer.GetCommandBuffer()->SetViewport({ Vector4(0, 0, s, s) }); break;
	case 1: renderer.GetCommandBuffer()->SetViewport({ Vector4(s, 0, s, s) }); break;
	case 2: renderer.GetCommandBuffer()->SetViewport({ Vector4(0, s, s, s) }); break;
	case 3: renderer.GetCommandBuffer()->SetViewport({ Vector4(s, s, s, s) }); break;
	}

	VERUS_RT_ASSERT(!_baking);
	_baking = true;
}

void CascadedShadowMapBaker::End(int split)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::End();

	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(_currentSplit == split);

	if (3 == _currentSplit)
		renderer.GetCommandBuffer()->EndRenderPass();

	const Matrix4 m = Math::ToUVMatrix();
	_matShadowCSM[_currentSplit] = _matOffset[_currentSplit] * m * _passCamera.GetMatrixVP();

	_pPrevPassCamera = wm.SetPassCamera(_pPrevPassCamera);
	VERUS_RT_ASSERT(&_passCamera == _pPrevPassCamera); // Check camera's integrity.
	_pPrevPassCamera = nullptr;

	_currentSplit = -1;

	VERUS_RT_ASSERT(_baking);
	_baking = false;
}

RcMatrix4 CascadedShadowMapBaker::GetShadowMatrix(int split) const
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::GetShadowMatrix();
	return _matShadowCSM[split];
}

RcMatrix4 CascadedShadowMapBaker::GetShadowMatrixDS(int split) const
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::GetShadowMatrixDS();
	return _matShadowCSM_DS[split];
}

PCamera CascadedShadowMapBaker::GetPassCameraCSM()
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return nullptr;
	return &_passCameraCSM;
}
