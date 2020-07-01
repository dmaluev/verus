#include "verus.h"

using namespace verus;
using namespace verus::GUI;

ViewController::ViewController()
{
}

ViewController::~ViewController()
{
	Done();
}

void ViewController::Init(CSZ url)
{
	VERUS_INIT();

	_pView = ViewManager::I().ParseView(url);
	_pView->SetDelegate(this);

#ifdef _DEBUG
	ConnectOnKey(VERUS_EVENT_HANDLER(&ViewController::OnKey));
#endif
}

void ViewController::Done()
{
	ViewManager::I().DeleteView(_pView);
	VERUS_DONE(ViewController);
}

void ViewController::Reload()
{
	const String url = _C(_pView->GetUrl());
	Done();
	Init(_C(url));
	BeginFadeTo();
}

PWidget ViewController::GetWidgetById(CSZ id)
{
	return _pView->GetWidgetById(id);
}

void ViewController::ConnectOnMouseEnter(TFnEvent fn, CSZ id)
{
	_pView->GetWidgetById(id)->ConnectOnMouseEnter(fn);
}

void ViewController::ConnectOnMouseLeave(TFnEvent fn, CSZ id)
{
	_pView->GetWidgetById(id)->ConnectOnMouseLeave(fn);
}

void ViewController::ConnectOnClick(TFnEvent fn, CSZ id)
{
	_pView->GetWidgetById(id)->ConnectOnClick(fn);
}

void ViewController::ConnectOnDoubleClick(TFnEvent fn, CSZ id)
{
	_pView->GetWidgetById(id)->ConnectOnDoubleClick(fn);
}

void ViewController::ConnectOnKey(TFnEvent fn, CSZ id)
{
	PWidget p = id ? _pView->GetWidgetById(id) : _pView;
	p->ConnectOnKey(fn);
}

void ViewController::ConnectOnChar(TFnEvent fn, CSZ id)
{
	PWidget p = id ? _pView->GetWidgetById(id) : _pView;
	p->ConnectOnChar(fn);
}

void ViewController::ConnectOnTimeout(TFnEvent fn, CSZ id)
{
	PWidget p = id ? _pView->GetWidgetById(id) : _pView;
	p->ConnectOnTimeout(fn);
}

void ViewController::BeginFadeTo()
{
	ViewManager::I().BeginFadeTo(_C(_pView->GetName()));
}

void ViewController::OnKey(RcEventArgs args)
{
	if (args._char == SDL_SCANCODE_F5)
		Reload();
}
