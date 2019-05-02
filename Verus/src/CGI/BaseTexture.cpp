#include "verus.h"

using namespace verus;
using namespace verus::CGI;

// TexturePtr:

void TexturePtr::Init(RcTextureDesc desc)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_p);
	_p = renderer->InsertTexture();
	//if (desc.m_url)
	//	_p->LoadDDS(desc.m_url, desc.m_texturePart);
	//else if (desc.m_urls)
	//	_p->LoadManyDDS(desc.m_urls);
	//else
	//	_p->Init(desc);
}

void TexturePwn::Done()
{
	if (_p)
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteTexture(_p);
		_p = nullptr;
	}
}

TexturePtr TexturePtr::From(PBaseTexture p)
{
	TexturePtr ptr;
	ptr._p = p;
	return ptr;
}
