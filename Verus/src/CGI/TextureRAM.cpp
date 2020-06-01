#include "verus.h"

using namespace verus;
using namespace verus::CGI;

TextureRAM::TextureRAM()
{
}

TextureRAM::~TextureRAM()
{
	Done();
}

void TextureRAM::Init(RcTextureDesc desc)
{
	VERUS_INIT();
	VERUS_RT_ASSERT(desc._width > 0 && desc._height > 0);
	VERUS_QREF_RENDERER;

	_size = Vector4(
		float(desc._width),
		float(desc._height),
		1.f / desc._width,
		1.f / desc._height);
	_desc = desc;
	_desc._mipLevels = desc._mipLevels ? desc._mipLevels : Math::ComputeMipLevels(desc._width, desc._height, desc._depth);
	_bytesPerPixel = FormatToBytesPerPixel(desc._format);

	_vBuffer.resize(_desc._width * _desc._height * _bytesPerPixel);
}

void TextureRAM::Done()
{
	VERUS_DONE(TextureRAM);
}

void TextureRAM::UpdateSubresource(const void* p, int mipLevel, int arrayLayer, PBaseCommandBuffer pCB)
{
	if (mipLevel || arrayLayer)
		return;
	memcpy(_vBuffer.data(), p, _vBuffer.size());
}

bool TextureRAM::ReadbackSubresource(void* p, PBaseCommandBuffer pCB)
{
	VERUS_RT_ASSERT(IsLoaded());
	memcpy(p, _vBuffer.data(), _vBuffer.size());
	return true;
}

void TextureRAM::GenerateMips(PBaseCommandBuffer pCB)
{
	VERUS_RT_ASSERT(__FUNCTION__);
}
