// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// ShadowMapBaker:

ShadowMapBaker::ShadowMapBaker()
{
	_matShadow = Matrix4::identity();
	_matShadowDS = Matrix4::identity();
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
	VERUS_QREF_SM;
	_matShadowDS = _matShadow * sm.GetViewCamera()->GetMatrixInvV();
}

void ShadowMapBaker::Begin(RcVector3 dirToSun)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	const Vector3 up(0, 1, 0);
	Point3 eye, at;
	const float texSizeInMeters = 4096 * 0.0075f;
	const float depth = texSizeInMeters * 10;
	const float zNear = 1;
	const float zFar = zNear + depth;

	at = sm.GetHeadCamera()->GetEyePosition() + sm.GetHeadCamera()->GetFrontDirection() * (texSizeInMeters * (1 / 3.f));
	if (_snapToTexels)
	{
		const Matrix4 matToLightSpace = Matrix4::lookAt(Point3(0), Point3(-dirToSun), up);
		const Matrix4 matFromLightSpace = VMath::orthoInverse(matToLightSpace);
		const Point3 atPosLightSpace = (matToLightSpace * at).getXYZ();
		const float texelSizeInMeters = texSizeInMeters / _side;
		Point3 atPosLightSpaceSnapped(atPosLightSpace);
		atPosLightSpaceSnapped.setX(atPosLightSpaceSnapped.getX() - fmod(atPosLightSpaceSnapped.getX(), texelSizeInMeters));
		atPosLightSpaceSnapped.setY(atPosLightSpaceSnapped.getY() - fmod(atPosLightSpaceSnapped.getY(), texelSizeInMeters));
		at = (matFromLightSpace * atPosLightSpaceSnapped).getXYZ();
	}
	eye = at + dirToSun * (depth * (2 / 3.f));

	// Setup light space camera and use it (used for terrain draw, etc.):
	_passCamera.MoveEyeTo(eye);
	_passCamera.MoveAtTo(at);
	_passCamera.SetUpDirection(up);
	_passCamera.SetYFov(0);
	_passCamera.SetZNear(zNear);
	_passCamera.SetZFar(zFar);
	_passCamera.SetXMag(texSizeInMeters);
	_passCamera.SetYMag(texSizeInMeters);
	_passCamera.Update();
	_pPrevPassCamera = sm.SetPassCamera(&_passCamera);

	_config._penumbraScale = depth * 2;

	renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh, { _tex->GetClearValue() },
		CGI::ViewportScissorFlags::setAllForFramebuffer);

	VERUS_RT_ASSERT(!_baking);
	_baking = true;
}

void ShadowMapBaker::End()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	renderer.GetCommandBuffer()->EndRenderPass();

	const Matrix4 m = Math::ToUVMatrix();
	_matShadow = m * _passCamera.GetMatrixVP();

	_pPrevPassCamera = sm.SetPassCamera(_pPrevPassCamera);
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
	_matShadowCSM[0] = Matrix4::identity();
	_matShadowCSM[1] = Matrix4::identity();
	_matShadowCSM[2] = Matrix4::identity();
	_matShadowCSM[3] = Matrix4::identity();
	_matShadowCSM_DS[0] = Matrix4::identity();
	_matShadowCSM_DS[1] = Matrix4::identity();
	_matShadowCSM_DS[2] = Matrix4::identity();
	_matShadowCSM_DS[3] = Matrix4::identity();
}

CascadedShadowMapBaker::~CascadedShadowMapBaker()
{
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

	VERUS_QREF_SM;
	VERUS_FOR(i, 4)
		_matShadowCSM_DS[i] = _matShadowCSM[i] * sm.GetViewCamera()->GetMatrixInvV();
}

void CascadedShadowMapBaker::Begin(RcVector3 dirToSun, int split)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMapBaker::Begin(dirToSun);

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	_currentSplit = split;

	RcCamera headCamera = *sm.GetHeadCamera();

	const Vector3 up(0, 1, 0);
	Point3 eye, at;
	float texWidthInMeters, texHeightInMeters, texSizeInMeters;

	float zNearFrustum, zFarFrustum;
	const Matrix4 matToLightSpace = Matrix4::lookAt(Point3(0), Point3(-dirToSun), up);
	const float maxRange = (settings._sceneShadowQuality >= App::Settings::Quality::ultra) ? 1000.f : 300.f;
	const float range = Math::Min<float>(maxRange, headCamera.GetZFar() - headCamera.GetZNear()); // Clip terrain, etc.
	_passCamera = headCamera;

	if (0 == _currentSplit)
	{
		_matScreenVP = _passCamera.GetMatrixVP();
		_matScreenP = _passCamera.GetMatrixP();

		const float depth = Math::Min(range * 10, 10000.f);
		const float zNear = 1;
		const float zFar = zNear + depth;

		_passCamera.SetZNear(headCamera.GetZNear());
		_passCamera.SetZFar(headCamera.GetZNear() + range);
		_passCamera.Update();

		const Vector4 frustumBounds = _passCamera.GetFrustum().GetBounds(matToLightSpace, zNearFrustum, zFarFrustum);
		Point3 pos(
			(frustumBounds.getX() + frustumBounds.getZ()) * 0.5f,
			(frustumBounds.getY() + frustumBounds.getW()) * 0.5f,
			zNearFrustum);
		pos = (VMath::orthoInverse(matToLightSpace) * pos).getXYZ(); // To world space.

		eye = pos + dirToSun * (depth * (2 / 3.f));
		at = pos;

		texWidthInMeters = ceil(frustumBounds.getZ() - frustumBounds.getX() + 2.5f);
		texHeightInMeters = ceil(frustumBounds.getW() - frustumBounds.getY() + 2.5f);
		texSizeInMeters = Math::Max(texWidthInMeters, texHeightInMeters);
		_depth = Math::Clamp<float>(texSizeInMeters * 10, 1000, 10000); // Must be the same for all splits.

		// Setup CSM light space camera for full range (used for terrain layout, etc.):
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

	// Compute split ranges:
	const float scale = 0.3f;
	const float scale2 = scale * scale;
	const float scale3 = scale2 * scale;
	const float invSum = 1.f / (1 + scale + scale2 + scale3);
	_splitRanges = Vector4(
		headCamera.GetZNear() + range * (invSum * (scale3 + scale2 + scale)),
		headCamera.GetZNear() + range * (invSum * (scale3 + scale2)),
		headCamera.GetZNear() + range * (invSum * (scale3)),
		headCamera.GetZNear());

	// Default scene camera with the current split range:
	switch (_currentSplit)
	{
	case 0:
		_passCamera.SetZNear(_splitRanges.getX());
		_passCamera.SetZFar(headCamera.GetZNear() + range);
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
	_passCamera.Update();

	const Vector4 frustumBounds = _passCamera.GetFrustum().GetBounds(matToLightSpace, zNearFrustum, zFarFrustum);
	Point3 pos(
		(frustumBounds.getX() + frustumBounds.getZ()) * 0.5f,
		(frustumBounds.getY() + frustumBounds.getW()) * 0.5f,
		zNearFrustum);

	texWidthInMeters = ceil(frustumBounds.getZ() - frustumBounds.getX() + 2.5f);
	texHeightInMeters = ceil(frustumBounds.getW() - frustumBounds.getY() + 2.5f);
	texSizeInMeters = Math::Max(texWidthInMeters, texHeightInMeters);
	const float zNear = 1;
	const float zFar = zNear + _depth;

	if (_snapToTexels)
	{
		const float invSide = 2.f / _side;
		const float texelSizeInMeters = texSizeInMeters * invSide;
		pos.setX(pos.getX() - fmod(pos.getX(), texelSizeInMeters));
		pos.setY(pos.getY() - fmod(pos.getY(), texelSizeInMeters));
	}
	pos = (VMath::orthoInverse(matToLightSpace) * pos).getXYZ(); // To world space.

	eye = pos + dirToSun * (_depth * (2 / 3.f));
	at = pos;

	_splitRanges.setW(headCamera.GetZNear() + range);

	// Setup light space camera and use it (used for terrain draw, etc.):
	_passCamera.MoveEyeTo(eye);
	_passCamera.MoveAtTo(at);
	_passCamera.SetUpDirection(up);
	_passCamera.SetYFov(0);
	_passCamera.SetZNear(zNear);
	_passCamera.SetZFar(zFar);
	_passCamera.SetXMag(texSizeInMeters);
	_passCamera.SetYMag(texSizeInMeters);
	_passCamera.Update();
	_pPrevPassCamera = sm.SetPassCamera(&_passCamera);

	_config._penumbraScale = (zFar - zNear) * 2;

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
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(_currentSplit == split);

	if (3 == _currentSplit)
		renderer.GetCommandBuffer()->EndRenderPass();

	const Matrix4 m = Math::ToUVMatrix();
	_matShadowCSM[_currentSplit] = _matOffset[_currentSplit] * m * _passCamera.GetMatrixVP();

	_pPrevPassCamera = sm.SetPassCamera(_pPrevPassCamera);
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
