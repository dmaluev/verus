#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

TextureVulkan::TextureVulkan()
{
}

TextureVulkan::~TextureVulkan()
{
	Done();
}

void TextureVulkan::Init(RcTextureDesc desc)
{
	VERUS_INIT();
}

void TextureVulkan::Done()
{
	VERUS_DONE(TextureVulkan);
}
