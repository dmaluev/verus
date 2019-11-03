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

	_rp = renderer->CreateRenderPass(
		{
			CGI::RP::Attachment("Depth", CGI::Format::unormD16).LoadOpClear().Layout(CGI::ImageLayout::depthStencilReadOnly),
		},
		{
			CGI::RP::Subpass("Sp0").Color({}).DepthStencil(CGI::RP::Ref("Depth", CGI::ImageLayout::depthStencilAttachment)),
		},
		{});

	CGI::TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._format = CGI::Format::unormD16;
	texDesc._width = _side;
	texDesc._height = _side;
	texDesc._flags = CGI::TextureDesc::Flags::depthSampled;
	_tex.Init(texDesc);

	_fb = renderer->CreateFramebuffer(_rp,
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

void ShadowMap::Begin(RcVector3 dirToSun, float zNear, float zFar)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	const Vector3 up(0, 1, 0);
	Point3 eye, at;
	const float size = 4096 * 0.01f;
	if (0 == zFar)
		zFar = 0x10000 * 0.005f;

	at = sm.GetCamera()->GetPositionEye() + sm.GetCamera()->GetDirectionFront() * (size * (1 / 3.f));
	if (_snapToTexels)
	{
		const Matrix4 matToShadowSpace = Matrix4::lookAt(Point3(dirToSun), Point3(0), up);
		const Matrix4 matFromShadowSpace = VMath::orthoInverse(matToShadowSpace);
		const Point3 posAtShadowSpace = (matToShadowSpace * at).getXYZ();
		const float texelSize = size / _side;
		Point3 posAtShadowSpaceSnapped(posAtShadowSpace);
		posAtShadowSpaceSnapped.setX(posAtShadowSpaceSnapped.getX() - fmod(static_cast<float>(posAtShadowSpaceSnapped.getX()), texelSize));
		posAtShadowSpaceSnapped.setY(posAtShadowSpaceSnapped.getY() - fmod(static_cast<float>(posAtShadowSpaceSnapped.getY()), texelSize));
		at = (matFromShadowSpace * posAtShadowSpaceSnapped).getXYZ();
	}
	eye = at + dirToSun * ((zNear + zFar) * 0.5f);

	// Setup light space camera and use it (used for terrain draw, etc.):
	_camera.SetDirectionUp(up);
	_camera.MoveAtTo(at);
	_camera.MoveEyeTo(eye);
	_camera.SetFOV(0);
	_camera.SetZNear(zNear);
	_camera.SetZFar(zFar);
	_camera.SetWidth(size);
	_camera.SetHeight(size);
	_camera.Update();
	_pSceneCamera = sm.SetCamera(&_camera);

	renderer.GetCommandBuffer()->BeginRenderPass(_rp, _fb,
		{
			_tex->GetClearValue()
		});

	_rendering = true;
}

void ShadowMap::BeginLight(RcPoint3 pos, RcPoint3 target, float side)
{
#if 0
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_SM;

	const float zFar = 100; // pos.DistanceTo(target)*1.5f;
	_camera.SetDirectionUp(Vector3(0, 1, 0));
	_camera.MoveAtTo(target);
	_camera.MoveEyeTo(pos);
	_camera.SetFOV(0);
	_camera.SetZNear(0.1f);
	_camera.SetZFar(zFar);
	_camera.SetWidth(side);
	_camera.SetHeight(side);
	_camera.Update();
	_pSceneCamera = sm.SetCamera(&_camera);

	render->SetRenderTargets({ _tex[TEX_COLOR] }, _tex[TEX_DEPTH]);
	if (1 == settings._gpuDepthTexture)
	{
		render->Clear(CGI::CLEAR_ZBUFFER);
	}
	else
	{
		render.SetClearColor(Vector4(1, 1, 1, 1));
		render->Clear(CGI::CLEAR_ZBUFFER | CGI::CLEAR_TARGET);
	}

	_rendering = true;
#endif
}

void ShadowMap::End()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_SM;

	renderer.GetCommandBuffer()->EndRenderPass();

	const Matrix4 m = Math::ToUVMatrix();
	_matShadow = m * _camera.GetMatrixVP();
	_matShadowDS = _matShadow * _pSceneCamera->GetMatrixVi();

	_pSceneCamera = sm.SetCamera(_pSceneCamera);
	VERUS_RT_ASSERT(&_camera == _pSceneCamera); // Check camera's integrity.
	_pSceneCamera = nullptr;

	_rendering = false;
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
	if (settings._sceneShadowQuality < App::Settings::ShadowQuality::cascaded)
		return ShadowMap::Init(side);

	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_side = side;

	_rp = renderer->CreateRenderPass(
		{
			CGI::RP::Attachment("Depth", CGI::Format::floatD32).LoadOpClear().Layout(CGI::ImageLayout::depthStencilReadOnly),
		},
		{
			CGI::RP::Subpass("Sp0").Color({}).DepthStencil(CGI::RP::Ref("Depth", CGI::ImageLayout::depthStencilAttachment)),
		},
		{});

	CGI::TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._format = CGI::Format::floatD32;
	texDesc._width = _side;
	texDesc._height = _side;
	texDesc._flags = CGI::TextureDesc::Flags::depthSampled;
	_tex.Init(texDesc);

	_fb = renderer->CreateFramebuffer(_rp,
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

void CascadedShadowMap::Begin(RcVector3 dirToSun, float zNear, float zFar, int split)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::ShadowQuality::cascaded)
		return ShadowMap::Begin(dirToSun, zNear, zFar);

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	_currentSplit = split;

	RcCamera cam = *sm.GetCamera();

	const Vector3 up(0, 1, 0);
	Point3 eye, at;
	float sizeW, sizeH;

	float zNearFrustum, zFarFrustum;
	const float closerToLight = 10000;
	const Matrix4 matToLightSpace = Matrix4::lookAt(Point3(0), Point3(-dirToSun), up);
	const float range = Math::Min(1000.f, cam.GetZFar() - cam.GetZNear()); // Clip terrain, etc.
	_camera = cam;

	if (0 == _currentSplit)
	{
		_camera.SetZNear(cam.GetZNear());
		_camera.SetZFar(cam.GetZNear() + range);
		_camera.Update();

		const Vector4 frustumBounds = _camera.GetFrustum().GetBounds(matToLightSpace, zNearFrustum, zFarFrustum);
		Point3 pos(
			(frustumBounds.getX() + frustumBounds.getZ()) * 0.5f,
			(frustumBounds.getY() + frustumBounds.getW()) * 0.5f,
			zNearFrustum);
		pos = (VMath::orthoInverse(matToLightSpace) * pos).getXYZ(); // To world space.

		eye = pos + dirToSun * closerToLight;
		at = pos;
		if (0 == zFar)
			zFar = abs(zFarFrustum - zNearFrustum) + closerToLight;

		sizeW = static_cast<float>(int(frustumBounds.getZ() - frustumBounds.getX() + 0.5f) + 2);
		sizeH = static_cast<float>(int(frustumBounds.getW() - frustumBounds.getY() + 0.5f) + 2);

		// Setup CSM light space camera for full range (used for terrain layout, etc.):
		_cameraCSM.SetDirectionUp(up);
		_cameraCSM.MoveAtTo(at);
		_cameraCSM.MoveEyeTo(eye);
		_cameraCSM.SetFOV(0);
		_cameraCSM.SetZNear(zNear);
		_cameraCSM.SetZFar(zFar);
		_cameraCSM.SetWidth(sizeW);
		_cameraCSM.SetHeight(sizeH);
		_cameraCSM.Update();
	}

	// Compute split ranges:
	_splitRanges = Vector4(
		cam.GetZNear() + range * 0.5f * 0.5f,
		cam.GetZNear() + range * 0.25f * 0.25f,
		cam.GetZNear() + range * 0.125f * 0.125f,
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

	sizeW = static_cast<float>(int(frustumBounds.getZ() - frustumBounds.getX() + 0.5f) + 2);
	sizeH = static_cast<float>(int(frustumBounds.getW() - frustumBounds.getY() + 0.5f) + 2);

	if (_snapToTexels)
	{
		const float sideInv = 2.f / _side;
		pos.setX(pos.getX() - fmod(static_cast<float>(pos.getX()), sizeW* sideInv));
		pos.setY(pos.getY() - fmod(static_cast<float>(pos.getY()), sizeH* sideInv));
	}
	pos = (VMath::orthoInverse(matToLightSpace) * pos).getXYZ(); // To world space.

	eye = pos + dirToSun * closerToLight;
	at = pos;
	if (0 == zFar)
		zFar = _cameraCSM.GetZFar();

	_splitRanges.setW(cam.GetZNear() + range);

	// Setup light space camera and use it (used for terrain draw, etc.):
	_camera.SetDirectionUp(up);
	_camera.MoveAtTo(at);
	_camera.MoveEyeTo(eye);
	_camera.SetFOV(0);
	_camera.SetZNear(zNear);
	_camera.SetZFar(zFar);
	_camera.SetWidth(sizeW);
	_camera.SetHeight(sizeH);
	_camera.Update();
	_pSceneCamera = sm.SetCamera(&_camera);

	if (0 == _currentSplit)
	{
		renderer.GetCommandBuffer()->BeginRenderPass(_rp, _fb,
			{
				_tex->GetClearValue()
			});
	}

	const int s = _side / 2;
	switch (_currentSplit)
	{
	case 0: renderer.GetCommandBuffer()->SetViewport({ Vector4(0, 0, s, s) }); break;
	case 1: renderer.GetCommandBuffer()->SetViewport({ Vector4(s, 0, s, s) }); break;
	case 2: renderer.GetCommandBuffer()->SetViewport({ Vector4(0, s, s, s) }); break;
	case 3: renderer.GetCommandBuffer()->SetViewport({ Vector4(s, s, s, s) }); break;
	}

	_rendering = true;
}

void CascadedShadowMap::End(int split)
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::ShadowQuality::cascaded)
		return ShadowMap::End();

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(_currentSplit == split);

	if (3 == _currentSplit)
		renderer.GetCommandBuffer()->EndRenderPass();

	const Matrix4 m = Math::ToUVMatrix();
	_matShadowCSM[_currentSplit] = _matOffset[_currentSplit] * m * _camera.GetMatrixVP();
	_matShadowCSM_DS[_currentSplit] = _matShadowCSM[_currentSplit] * _pSceneCamera->GetMatrixVi();

	_pSceneCamera = sm.SetCamera(_pSceneCamera);
	VERUS_RT_ASSERT(&_camera == _pSceneCamera); // Check camera's integrity.
	_pSceneCamera = nullptr;

	_rendering = false;
}

RcMatrix4 CascadedShadowMap::GetShadowMatrix(int split) const
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::ShadowQuality::cascaded)
		return ShadowMap::GetShadowMatrix();
	return _matShadowCSM[split];
}

RcMatrix4 CascadedShadowMap::GetShadowMatrixDS(int split) const
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::ShadowQuality::cascaded)
		return ShadowMap::GetShadowMatrixDS();
	return _matShadowCSM_DS[split];
}

PCamera CascadedShadowMap::GetCameraCSM()
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneShadowQuality < App::Settings::ShadowQuality::cascaded)
		return nullptr;
	return &_cameraCSM;
}
