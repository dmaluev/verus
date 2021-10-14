// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

ViewManager::ViewManager()
{
}

ViewManager::~ViewManager()
{
	Done();
}

void ViewManager::Init(bool hasCursor, bool canDebug)
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	_shader.Init("[Shaders]:GUI.hlsl");
	_shader->CreateDescriptorSet(0, &_ubGui, sizeof(_ubGui), settings.GetLimits()._gui_ubGuiCapacity);
	_shader->CreateDescriptorSet(1, &_ubGuiFS, sizeof(_ubGuiFS), settings.GetLimits()._gui_ubGuiFSCapacity,
		{
			CGI::Sampler::anisoSharp
		}, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::TextureDesc texDesc;
		texDesc._name = "ViewManager.Dummy";
		texDesc._format = CGI::Format::unormR8G8B8A8;
		texDesc._width = 8;
		texDesc._height = 8;
		_tex[TEX_DUMMY].Init(texDesc);
	}

	_cshDefault = _shader->BindDescriptorSetTextures(1, { _tex[TEX_DUMMY] });

	if (canDebug)
		_tex[TEX_DEBUG].Init("[Textures]:UI/Debug.dds");

	if (hasCursor)
		_cursor.Init();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#", renderer.GetRenderPassHandle_AutoWithDepth());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ALPHA;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_MAIN].Init(pipeDesc);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
		_pipe[PIPE_MAIN_ADD].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#Mask", renderer.GetRenderPassHandle_AutoWithDepth());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ALPHA;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_MASK].Init(pipeDesc);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
		_pipe[PIPE_MASK_ADD].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#SolidColor", renderer.GetRenderPassHandle_AutoWithDepth());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ALPHA;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_SOLID_COLOR].Init(pipeDesc);
	}
}

void ViewManager::Done()
{
	DeleteAllViews();
	DeleteAllFonts();

	_cursor.Done();

	_shader->FreeDescriptorSet(_cshDebug);
	_shader->FreeDescriptorSet(_cshDefault);

	VERUS_DONE(ViewManager);
}

void ViewManager::HandleInput()
{
	if (!_vViews.empty())
	{
		VERUS_QREF_IM;
		if (im.IsMouseDownEvent(SDL_BUTTON_LEFT))
			_vViews[0]->OnClick();
		if (im.IsMouseDoubleClick(SDL_BUTTON_LEFT))
			_vViews[0]->OnDoubleClick();
	}
}

bool ViewManager::Update()
{
	VERUS_UPDATE_ONCE_CHECK;

	if (!_cshDebug.IsSet() && _tex[TEX_DEBUG]->IsLoaded())
		_cshDebug = _shader->BindDescriptorSetTextures(1, { _tex[TEX_DEBUG] });

	for (auto& x : TStoreFonts::_map)
		x.second.ResetDynamicBuffer();

	for (const auto& p : _vViews)
		p->Update();

	return SwitchView();
}

void ViewManager::Draw()
{
	VERUS_UPDATE_ONCE_CHECK_DRAW;

	PView pView = nullptr;
	VERUS_FOREACH_REVERSE_CONST(Vector<PView>, _vViews, it)
	{
		pView = *it;
		pView->Draw();
	}
	if (pView && pView->HasCursor())
		_cursor.Draw();
}

PView ViewManager::ParseView(CSZ url)
{
	CSZ name = strchr(url, ':') + 1;
	VERUS_RT_ASSERT(!GetViewByName(name));

	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(url, vData);

	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(vData.data(), vData.size(), pugi::parse_default & ~pugi::parse_wconv_attribute);
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(); " << result.description();
	pugi::xml_node root = doc.first_child();

	PView pView = new View();
	_vViews.push_back(pView);

	_pCurrentParseView = pView;
	pView->_url = url;
	pView->_name = name;
	pView->Parse(root);
	pView->Disable();
	pView->Hide();

	return pView;
}

void ViewManager::DeleteView(PView pView)
{
	VERUS_FOREACH(Vector<PView>, _vViews, it)
	{
		if (*it == pView)
		{
			delete pView;
			_vViews.erase(it);
			return;
		}
	}
}

void ViewManager::DeleteAllViews()
{
	for (const auto& x : _vViews)
		delete x;
	_vViews.clear();
}

PFont ViewManager::LoadFont(CSZ url)
{
	String name;
	if (strrchr(url, '/'))
		name.assign(strrchr(url, '/') + 1);
	else
		name.assign(strrchr(url, ':') + 1);
	name = name.substr(0, name.length() - 4);

	PFont pFont = FindFont(_C(name));
	if (pFont)
		return pFont;

	pFont = TStoreFonts::Insert(name);
	pFont->Init(url);
	return pFont;
}

PFont ViewManager::FindFont(CSZ name)
{
	if (name && strlen(name) > 0)
		return TStoreFonts::Find(name);
	else
		return &TStoreFonts::_map.begin()->second;
}

void ViewManager::DeleteFont(CSZ name)
{
	TStoreFonts::Delete(name);
}

void ViewManager::DeleteAllFonts()
{
	TStoreFonts::DeleteAll();
}

void ViewManager::MsgBox(CSZ txt, int data)
{
	if (txt) // Display MessageBox:
	{
		PView pView = Activate("UI/MsgBox.xml");
		PLabel pLabel = static_cast<PLabel>(pView->GetWidgetById("MsgBoxText"));
		pLabel->SetText(txt);
	}
	else // Restore original view:
	{
		PView pView = Deactivate("UI/MsgBox.xml");
	}
}

void ViewManager::MsgBoxOK(CSZ id)
{
	//_pGUIDelegate->GUI_MessageBoxOK(_currentViewID, id);
}

PView ViewManager::GetViewByName(CSZ name)
{
	for (const auto& pView : _vViews)
	{
		if (pView->_name == name)
			return pView;
	}
	return nullptr;
}

PView ViewManager::MoveToFront(CSZ viewName)
{
	if (_vViews[0]->_pLastHovered)
	{
		_vViews[0]->_pLastHovered->InvokeOnMouseLeave();
		_vViews[0]->_pLastHovered = nullptr;
	}

	std::stable_sort(_vViews.begin(), _vViews.end(), [viewName](PView pA, PView pB)
		{
			const int a = (pA->GetName() == viewName) ? 0 : 1;
			const int b = (pB->GetName() == viewName) ? 0 : 1;
			return a < b;
		});
	if (_vViews.front()->GetName() == viewName)
	{
		_vViews.front()->_pLastHovered = nullptr;
		return _vViews.front();
	}
	return nullptr;
}

PView ViewManager::MoveToBack(CSZ viewName)
{
	if (_vViews[0]->_pLastHovered)
	{
		_vViews[0]->_pLastHovered->InvokeOnMouseLeave();
		_vViews[0]->_pLastHovered = nullptr;
	}

	std::stable_sort(_vViews.begin(), _vViews.end(), [viewName](PView pA, PView pB)
		{
			const int a = (pA->GetName() == viewName) ? 1 : 0;
			const int b = (pB->GetName() == viewName) ? 1 : 0;
			return a < b;
		});
	if (_vViews.back()->GetName() == viewName)
	{
		_vViews.back()->_pLastHovered = nullptr;
		return _vViews.back();
	}
	return nullptr;
}

PView ViewManager::BeginFadeTo(CSZ viewName)
{
	if (_fadeToView == viewName)
		return nullptr;

	int visibleCount = 0;
	for (const auto& pView : _vViews)
	{
		if (View::State::done != pView->GetState())
		{
			pView->BeginFadeOut(); // Put all views into done state.
			visibleCount++;
		}
	}
	_fadeToView = viewName;

	if (!visibleCount)
		SwitchView();

	return GetViewByName(viewName);
}

void ViewManager::BeginFadeOut()
{
	for (const auto& pView : _vViews)
	{
		if (View::State::done != pView->GetState())
			pView->BeginFadeOut(); // Put all views into done state.
	}
}

PView ViewManager::Activate(CSZ viewName)
{
	PView pView = GetViewByName(viewName);
	pView->Disable(false);
	pView->Show();
	pView->SetData();
	pView->ResetAnimators();
	pView->SetState(View::State::active);
	MoveToFront(viewName);
	return pView;
}

PView ViewManager::Deactivate(CSZ viewName)
{
	PView pView = GetViewByName(viewName);
	pView->Disable();
	pView->Hide();
	pView->SetState(View::State::done);
	MoveToBack(viewName);
	return pView;
}

bool ViewManager::HasAllViewsInDoneState()
{
	for (const auto& pView : _vViews)
		if (View::State::done != pView->GetState())
			return false;
	return true;
}

bool ViewManager::HasSomeViewsInFadeState()
{
	for (const auto& pView : _vViews)
		if (View::State::fadeIn == pView->GetState() ||
			View::State::fadeOut == pView->GetState())
			return true;
	return false;
}

bool ViewManager::SwitchView()
{
	if (!_fadeToView.empty() && HasAllViewsInDoneState()) // Fade in progress:
	{
		for (const auto& pView : _vViews)
		{
			if (pView->IsVisible())
				pView->GetData();
			pView->Disable();
			pView->Hide();
		}
		PView pView = MoveToFront(_C(_fadeToView));
		if (pView)
		{
			pView->Disable(false);
			pView->Show();
			pView->SetData();
			pView->ResetAnimators();
			pView->BeginFadeIn();
		}
		_fadeToView.clear();
		return true;
	}
	return false;
}

void ViewManager::BindPipeline(PIPE pipe, CGI::CommandBufferPtr cb)
{
	cb->BindPipeline(_pipe[pipe]);
}

CGI::TexturePtr ViewManager::GetDebugTexture()
{
	return _tex[TEX_DEBUG];
}

float ViewManager::ParseCoordX(CSZ coord, float def)
{
	if (!coord || !strlen(coord))
		return def;
	if (Str::EndsWith(coord, "px"))
		return static_cast<float>(atof(coord) * (1 / 1920.f));
	if (Str::EndsWith(coord, "%"))
		return static_cast<float>(atof(coord)) * 0.01f;
	return static_cast<float>(atof(coord));
}

float ViewManager::ParseCoordY(CSZ coord, float def)
{
	if (!coord || !strlen(coord))
		return def;
	if (Str::EndsWith(coord, "px"))
		return static_cast<float>(atof(coord) * (1 / 1080.f));
	if (Str::EndsWith(coord, "%"))
		return static_cast<float>(atof(coord)) * 0.01f;
	return static_cast<float>(atof(coord));
}

void ViewManager::OnKey(int scancode)
{
	if (!_vViews.empty())
		_vViews[0]->OnKey(scancode);
}

void ViewManager::OnChar(wchar_t c)
{
	if (!_vViews.empty())
		_vViews[0]->OnChar(c);
}
