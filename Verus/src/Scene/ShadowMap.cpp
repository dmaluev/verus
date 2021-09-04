// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// ShadowMap:

ShadowMap::ShadowMap()
{
	_matShadow = Matrix4::identity();
	_matShadowDS = Matrix4::identity();
}

ShadowMap::~ShadowMap()
{
	Done();
}

void ShadowMap::Init(int side)
{
	VERUS_INIT();

	VERUS_QREF_RENDERER;

	_side = side;

	_rph = renderer->CreateShadowRenderPass(CGI::Format::unormD24uintS8);

	CGI::TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._name = "ShadowMap.Tex";
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

void ShadowMap::Done()
{
	VERUS_DONE(ShadowMap);
}

void ShadowMap::Begin(RcVector3 dirToSun)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	const Vector3 up(0, 1, 0);
	Point3 eye, at;
	const float texSizeInMeters = 4096 * 0.0075f;
	const float depth = texSizeInMeters * 10;
	const float zNear = 1;
	const float zFar = zNear + depth;

	at = sm.GetCamera()->GetEyePosition() + sm.GetCamera()->GetFrontDirection() * (texSizeInMeters * (1 / 3.f));
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
	_camera.MoveEyeTo(eye);
	_camera.MoveAtTo(at);
	_camera.SetUpDirection(up);
	_camera.SetYFov(0);
	_camera.SetZNear(zNear);
	_camera.SetZFar(zFar);
	_camera.SetXMag(texSizeInMeters);
	_camera.SetYMag(texSizeInMeters);
	_camera.Update();
	_pPrevCamera = sm.SetCamera(&_camera);

	_config._penumbraScale = depth * 2;

	renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh,
		{
			_tex->GetClearValue()
		});
}

void ShadowMap::End()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	renderer.GetCommandBuffer()->EndRenderPass();

	const Matrix4 m = Math::ToUVMatrix();
	_matShadow = m * _camera.GetMatrixVP();
	_matShadowDS = _matShadow * _pPrevCamera->GetMatrixInvV();

	_pPrevCamera = sm.SetCamera(_pPrevCamera);
	VERUS_RT_ASSERT(&_camera == _pPrevCamera); // Check camera's integrity.
	_pPrevCamera = nullptr;
}

RcMatrix4 ShadowMap::GetShadowMatrix() const
{
	return _matShadow;
}

RcMatrix4 ShadowMap::GetShadowMatrixDS() const
{
	return _matShadowDS;
}

CGI::TexturePtr ShadowMap::GetTexture() const
{
	if (!IsInitialized())
		return nullptr;
	return _tex;
}

// CascadedShadowMap::

CascadedShadowMap::CascadedShadowMap()
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

CascadedShadowMap::~CascadedShadowMap()
{
}

void CascadedShadowMap::Init(int side)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMap::Init(side);

	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_side = side;
	if (settings._sceneShadowQuality >= App::Settings::Quality::ultra)
		_side *= 2;

	_rph = renderer->CreateShadowRenderPass(CGI::Format::unormD24uintS8);

	CGI::TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._name = "ShadowMap.Tex (CSM)";
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

void CascadedShadowMap::Done()
{
}

void CascadedShadowMap::Begin(RcVector3 dirToSun, int split)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMap::Begin(dirToSun);

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	_currentSplit = split;

	RcCamera cam = *sm.GetCamera();

	const Vector3 up(0, 1, 0);
	Point3 eye, at;
	float texWidthInMeters, texHeightInMeters, texSizeInMeters;

	float zNearFrustum, zFarFrustum;
	const Matrix4 matToLightSpace = Matrix4::lookAt(Point3(0), Point3(-dirToSun), up);
	const float maxRange = (settings._sceneShadowQuality >= App::Settings::Quality::ultra) ? 1000.f : 300.f;
	const float range = Math::Min<float>(maxRange, cam.GetZFar() - cam.GetZNear()); // Clip terrain, etc.
	_camera = cam;

	if (0 == _currentSplit)
	{
		_matScreenVP = _camera.GetMatrixVP();
		_matScreenP = _camera.GetMatrixP();

		const float depth = Math::Min(range * 10, 10000.f);
		const float zNear = 1;
		const float zFar = zNear + depth;

		_camera.SetZNear(cam.GetZNear());
		_camera.SetZFar(cam.GetZNear() + range);
		_camera.Update();

		const Vector4 frustumBounds = _camera.GetFrustum().GetBounds(matToLightSpace, zNearFrustum, zFarFrustum);
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
		_cameraCSM.MoveEyeTo(eye);
		_cameraCSM.MoveAtTo(at);
		_cameraCSM.SetUpDirection(up);
		_cameraCSM.SetYFov(0);
		_cameraCSM.SetZNear(zNear);
		_cameraCSM.SetZFar(zFar);
		_cameraCSM.SetXMag(texSizeInMeters);
		_cameraCSM.SetYMag(texSizeInMeters);
		_cameraCSM.Update();
	}

	// Compute split ranges:
	const float scale = 0.3f;
	const float scale2 = scale * scale;
	const float scale3 = scale2 * scale;
	const float invSum = 1.f / (1 + scale + scale2 + scale3);
	_splitRanges = Vector4(
		cam.GetZNear() + range * (invSum * (scale3 + scale2 + scale)),
		cam.GetZNear() + range * (invSum * (scale3 + scale2)),
		cam.GetZNear() + range * (invSum * (scale3)),
		cam.GetZNear());

	// Default scene camera with the current split range:
	switch (_currentSplit)
	{
	case 0:
		_camera.SetZNear(_splitRanges.getX());
		_camera.SetZFar(cam.GetZNear() + range);
		break;
	case 1:
		_camera.SetZNear(_splitRanges.getY());
		_camera.SetZFar(_splitRanges.getX());
		break;
	case 2:
		_camera.SetZNear(_splitRanges.getZ());
		_camera.SetZFar(_splitRanges.getY());
		break;
	case 3:
		_camera.SetZNear(_splitRanges.getW());
		_camera.SetZFar(_splitRanges.getZ());
		break;
	}
	_camera.Update();

	const Vector4 frustumBounds = _camera.GetFrustum().GetBounds(matToLightSpace, zNearFrustum, zFarFrustum);
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

	_splitRanges.setW(cam.GetZNear() + range);

	// Setup light space camera and use it (used for terrain draw, etc.):
	_camera.MoveEyeTo(eye);
	_camera.MoveAtTo(at);
	_camera.SetUpDirection(up);
	_camera.SetYFov(0);
	_camera.SetZNear(zNear);
	_camera.SetZFar(zFar);
	_camera.SetXMag(texSizeInMeters);
	_camera.SetYMag(texSizeInMeters);
	_camera.Update();
	_pPrevCamera = sm.SetCamera(&_camera);

	_config._penumbraScale = (zFar - zNear) * 2;

	if (0 == _currentSplit)
	{
		renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh,
			{
				_tex->GetClearValue()
			});
	}

	const float s = static_cast<float>(_side / 2);
	switch (_currentSplit)
	{
	case 0: renderer.GetCommandBuffer()->SetViewport({ Vector4(0, 0, s, s) }); break;
	case 1: renderer.GetCommandBuffer()->SetViewport({ Vector4(s, 0, s, s) }); break;
	case 2: renderer.GetCommandBuffer()->SetViewport({ Vector4(0, s, s, s) }); break;
	case 3: renderer.GetCommandBuffer()->SetViewport({ Vector4(s, s, s, s) }); break;
	}
}

void CascadedShadowMap::End(int split)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMap::End();

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(_currentSplit == split);

	if (3 == _currentSplit)
		renderer.GetCommandBuffer()->EndRenderPass();

	const Matrix4 m = Math::ToUVMatrix();
	_matShadowCSM[_currentSplit] = _matOffset[_currentSplit] * m * _camera.GetMatrixVP();
	_matShadowCSM_DS[_currentSplit] = _matShadowCSM[_currentSplit] * _pPrevCamera->GetMatrixInvV();

	_pPrevCamera = sm.SetCamera(_pPrevCamera);
	VERUS_RT_ASSERT(&_camera == _pPrevCamera); // Check camera's integrity.
	_pPrevCamera = nullptr;

	_currentSplit = -1;
}

RcMatrix4 CascadedShadowMap::GetShadowMatrix(int split) const
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMap::GetShadowMatrix();
	return _matShadowCSM[split];
}

RcMatrix4 CascadedShadowMap::GetShadowMatrixDS(int split) const
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return ShadowMap::GetShadowMatrixDS();
	return _matShadowCSM_DS[split];
}

PCamera CascadedShadowMap::GetCameraCSM()
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::Quality::high)
		return nullptr;
	return &_cameraCSM;
}
