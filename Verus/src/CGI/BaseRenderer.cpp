#include "verus.h"

using namespace verus;
using namespace verus::CGI;

BaseRenderer::BaseRenderer()
{
}

BaseRenderer::~BaseRenderer()
{
}

PBaseRenderer BaseRenderer::Load(CSZ dll, RBaseRendererDesc desc)
{
#ifdef _WIN32
	PFNVERUSCREATERENDERER VerusCreateRenderer = reinterpret_cast<PFNVERUSCREATERENDERER>(
		GetProcAddress(LoadLibraryA(dll), "VerusCreateRenderer"));
	return VerusCreateRenderer(VERUS_SDK_VERSION, &desc);
#else
	PFNVERUSCREATERENDERER VerusCreateRenderer = reinterpret_cast<PFNVERUSCREATERENDERER>(
		dlsym(dlopen("./libRenderOpenGL.so", RTLD_LAZY), "VerusCreateRenderer"));
	return VerusCreateRenderer(VERUS_SDK_VERSION, &desc);
#endif
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
		alphaBlendOp = colorBlendOp;
		srcAlphaBlendFactor = srcColorBlendFactor;
		dstAlphaBlendFactor = dstColorBlendFactor;
	}
}
