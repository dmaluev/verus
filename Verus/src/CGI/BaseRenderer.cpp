// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::CGI;

BaseRenderer::BaseRenderer()
{
	_vScheduled.reserve(128);
}

BaseRenderer::~BaseRenderer()
{
}

PBaseRenderer BaseRenderer::Load(CSZ dll, RBaseRendererDesc desc)
{
#ifdef _WIN32
	PFNCREATERENDERER CreateRenderer = reinterpret_cast<PFNCREATERENDERER>(
		GetProcAddress(LoadLibraryA(dll), "CreateRenderer"));
	return CreateRenderer(VERUS_SDK_VERSION, &desc);
#else
	PFNCREATERENDERER CreateRenderer = reinterpret_cast<PFNCREATERENDERER>(
		dlsym(dlopen("./libRenderOpenGL.so", RTLD_LAZY), "CreateRenderer"));
	return CreateRenderer(VERUS_SDK_VERSION, &desc);
#endif
}

void BaseRenderer::Schedule(PScheduled p)
{
	if (_vScheduled.end() != std::find(_vScheduled.begin(), _vScheduled.end(), p))
		return;
	_vScheduled.push_back(p);
}

void BaseRenderer::Unschedule(PScheduled p)
{
	_vScheduled.erase(std::remove(_vScheduled.begin(), _vScheduled.end(), p), _vScheduled.end());
}

void BaseRenderer::UpdateScheduled()
{
	_vScheduled.erase(std::remove_if(_vScheduled.begin(), _vScheduled.end(), [](PScheduled p)
		{
			return Continue::no == p->Scheduled_Update();
		}), _vScheduled.end());
}

RPHandle BaseRenderer::CreateSimpleRenderPass(Format format, RP::Attachment::LoadOp loadOp, ImageLayout layout)
{
	return CreateRenderPass(
		{
			RP::Attachment("Attach", format).SetLoadOp(loadOp).Layout(layout),
		},
		{
			RP::Subpass("Sp0").Color({RP::Ref("Attach", ImageLayout::colorAttachment)}),
		},
		{});
}

RPHandle BaseRenderer::CreateShadowRenderPass(Format format)
{
	return CreateRenderPass(
		{
			RP::Attachment("Depth", format).LoadOpClear().Layout(ImageLayout::depthStencilReadOnly),
		},
		{
			RP::Subpass("Sp0").Color({}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachment)),
		},
		{});
}

void BaseRenderer::SetAlphaBlendHelper(
	CSZ sz,
	int& colorBlendOp,
	int& alphaBlendOp,
	int& srcColorBlendFactor,
	int& dstColorBlendFactor,
	int& srcAlphaBlendFactor,
	int& dstAlphaBlendFactor)
{
	enum BLENDOP
	{
		BLENDOP_ADD,
		BLENDOP_SUBTRACT,
		BLENDOP_REVSUBTRACT,
		BLENDOP_MIN,
		BLENDOP_MAX
	};

	colorBlendOp = -1;
	alphaBlendOp = -1;
	srcColorBlendFactor = 0;
	dstColorBlendFactor = 0;
	srcAlphaBlendFactor = 0;
	dstAlphaBlendFactor = 0;

	CSZ p = sz;

	static CSZ ppMinMax[] = { "min(", "max(", nullptr };
	if ('m' == *p)
	{
		switch (RendererParser::Expect(p, ppMinMax))
		{
		case 0: colorBlendOp = BLENDOP_MIN; break;
		case 1: colorBlendOp = BLENDOP_MAX; break;
		}
		alphaBlendOp = colorBlendOp;
		return;
	}
	static CSZ ppInvert[] = { "s*(", "d*(", nullptr };
	switch (RendererParser::Expect(p, ppInvert))
	{
	case 0: break;
	case 1: colorBlendOp = BLENDOP_REVSUBTRACT; break;
	default: VERUS_RT_FAIL("Color s/d not found.");
	}
	srcColorBlendFactor = RendererParser::ExpectBlendOp(p);
	p += VERUS_LITERAL_LENGTH(")");
	static CSZ ppSign[] = { "+", "-", ",", nullptr };
	switch (RendererParser::Expect(p, ppSign))
	{
	case 0: if (-1 == colorBlendOp) colorBlendOp = BLENDOP_ADD; break;
	case 1: if (-1 == colorBlendOp) colorBlendOp = BLENDOP_SUBTRACT; break;
	case 2: break;
	default: VERUS_RT_FAIL("Invalid color op.");
	}
	p += VERUS_LITERAL_LENGTH("d*(");
	dstColorBlendFactor = RendererParser::ExpectBlendOp(p);
	p += VERUS_LITERAL_LENGTH(")");
	if (BLENDOP_MIN == colorBlendOp || BLENDOP_MAX == colorBlendOp)
		p += VERUS_LITERAL_LENGTH(")");

	if ('|' == *p)
	{
		if ('m' == *p)
		{
			switch (RendererParser::Expect(p, ppMinMax))
			{
			case 0: alphaBlendOp = BLENDOP_MIN; break;
			case 1: alphaBlendOp = BLENDOP_MAX; break;
			}
		}
		switch (RendererParser::Expect(p, ppInvert))
		{
		case 0: break;
		case 1: alphaBlendOp = BLENDOP_REVSUBTRACT; break;
		default: VERUS_RT_FAIL("Alpha s/d not found.");
		}
		srcAlphaBlendFactor = RendererParser::ExpectBlendOp(p);
		p += VERUS_LITERAL_LENGTH(")");
		switch (RendererParser::Expect(p, ppSign))
		{
		case 0: if (-1 == alphaBlendOp) alphaBlendOp = BLENDOP_ADD; break;
		case 1: if (-1 == alphaBlendOp) alphaBlendOp = BLENDOP_SUBTRACT; break;
		case 2: break;
		default: VERUS_RT_FAIL("Invalid alpha op.");
		}
		p += VERUS_LITERAL_LENGTH("d*(");
		dstAlphaBlendFactor = RendererParser::ExpectBlendOp(p);
		p += VERUS_LITERAL_LENGTH(")");
	}
	else
	{
		alphaBlendOp = 0;
		srcAlphaBlendFactor = 1;
		dstAlphaBlendFactor = 0;
	}
}
