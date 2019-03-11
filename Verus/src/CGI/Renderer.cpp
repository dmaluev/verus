#include "verus.h"

using namespace verus;
using namespace verus::CGI;

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
	Done();
}

PBaseRenderer Renderer::operator->()
{
	VERUS_RT_ASSERT(_pBaseRenderer);
	return _pBaseRenderer;
}

bool Renderer::IsLoaded()
{
	return IsValidSingleton() && !!I()._pBaseRenderer;
}

void Renderer::Init(PRendererDelegate pDelegate)
{
	VERUS_INIT();

	VERUS_QREF_SETTINGS;

	_pRendererDelegate = pDelegate;

	CSZ dll = "RendererDirect3D12.dll";
	BaseRendererDesc desc;

	_pBaseRenderer = BaseRenderer::Load(dll, desc);

	_gapi = _pBaseRenderer->GetGapi();
}

void Renderer::Done()
{
	if (_pBaseRenderer)
	{
		_pBaseRenderer->ReleaseMe();
		_pBaseRenderer = nullptr;
	}

	VERUS_SMART_DELETE(_pRendererDelegate);

	VERUS_DONE(Renderer);
}

void Renderer::Draw()
{
	if (_pRendererDelegate)
		_pRendererDelegate->Renderer_OnDraw();
}

void Renderer::Present()
{
	if (_pRendererDelegate)
		_pRendererDelegate->Renderer_OnPresent();
	_numFrames++;
}
