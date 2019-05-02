#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

TextureD3D12::TextureD3D12()
{
}

TextureD3D12::~TextureD3D12()
{
	Done();
}

void TextureD3D12::Init(RcTextureDesc desc)
{
	VERUS_INIT();
}

void TextureD3D12::Done()
{
	VERUS_DONE(TextureD3D12);
}
