// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CubeMapBaker::CubeMapBaker()
{
}

CubeMapBaker::~CubeMapBaker()
{
	Done();
}

void CubeMapBaker::Init(int side)
{
	VERUS_INIT();

	VERUS_QREF_RENDERER;

	_side = side;

	_rph = renderer->CreateRenderPass(
		{
			CGI::RP::Attachment("Color", CGI::Format::floatR11G11B10).LoadOpDontCare().Layout(CGI::ImageLayout::fsReadOnly),
			CGI::RP::Attachment("Depth", CGI::Format::unormD24uintS8).LoadOpClear().Layout(CGI::ImageLayout::depthStencilAttachment),
		},
		{
			CGI::RP::Subpass("Sp0").Color(
				{
					CGI::RP::Ref("Color", CGI::ImageLayout::colorAttachment)
				}).DepthStencil(CGI::RP::Ref("Depth", CGI::ImageLayout::depthStencilAttachment)),
		},
		{});

	CGI::TextureDesc texDesc;
	texDesc._name = "CubeMapBaker.ColorTex";
	texDesc._format = CGI::Format::floatR11G11B10;
	texDesc._width = _side;
	texDesc._height = _side;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::generateMips | CGI::TextureDesc::Flags::cubeMap;
	_tex[TEX_COLOR].Init(texDesc);
	texDesc.Reset();
	texDesc._name = "CubeMapBaker.DepthTex";
	texDesc._clearValue = Vector4(1);
	texDesc._format = CGI::Format::unormD24uintS8;
	texDesc._width = _side;
	texDesc._height = _side;
	_tex[TEX_DEPTH].Init(texDesc);

	VERUS_FOR(i, +CGI::CubeMapFace::count)
	{
		_fbh[i] = renderer->CreateFramebuffer(_rph,
			{
				_tex[TEX_COLOR],
				_tex[TEX_DEPTH]
			},
			_side,
			_side,
			-1,
			static_cast<CGI::CubeMapFace>(i));
	}
}

void CubeMapBaker::Done()
{
	VERUS_DONE(CubeMapBaker);
}

void CubeMapBaker::BeginEnvMap(CGI::CubeMapFace cubeMapFace, RcPoint3 center)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	_center = center;

	Vector3 facePos = Vector3(0);
	Vector3 up = Vector3(0, 1, 0);
	switch (cubeMapFace)
	{
	case CGI::CubeMapFace::posX: facePos = Vector3(+1, 0, 0); break;
	case CGI::CubeMapFace::negX: facePos = Vector3(-1, 0, 0); break;
	case CGI::CubeMapFace::posY: facePos = Vector3(0, +1, 0); up = Vector3(0, 0, -1); break;
	case CGI::CubeMapFace::negY: facePos = Vector3(0, -1, 0); up = Vector3(0, 0, +1); break;
	case CGI::CubeMapFace::posZ: facePos = Vector3(0, 0, +1); break;
	case CGI::CubeMapFace::negZ: facePos = Vector3(0, 0, -1); break;
	}

	_passCamera.MoveEyeTo(center);
	_passCamera.MoveAtTo(center - facePos); // Looking at center.
	_passCamera.SetUpDirection(up);
	_passCamera.SetYFov(VERUS_PI * -0.5f); // Using left-handed coordinate system!
	_passCamera.SetAspectRatio(1);
	_passCamera.SetZNear(1);
	_passCamera.SetZFar(200);
	_passCamera.Update();

	_pPrevPassCamera = sm.SetPassCamera(&_passCamera);

	renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh[+cubeMapFace],
		{
			_tex[TEX_COLOR]->GetClearValue(),
			_tex[TEX_DEPTH]->GetClearValue()
		},
		CGI::ViewportScissorFlags::setAllForFramebuffer);
}

void CubeMapBaker::EndEnvMap()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	renderer.GetCommandBuffer()->EndRenderPass();

	sm.SetPassCamera(_pPrevPassCamera);
}

CGI::TexturePtr CubeMapBaker::GetColorTexture()
{
	return _tex[TEX_COLOR];
}

CGI::TexturePtr CubeMapBaker::GetDepthTexture()
{
	return _tex[TEX_DEPTH];
}
