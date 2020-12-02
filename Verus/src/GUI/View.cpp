// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

View::View()
{
	_fade.Set(1, -_fadeSpeed);
	_fade.SetLimits(0, 1);
	_locale = App::Settings::IConst()._uiLang;
}

View::~View()
{
	ViewManager::I().GetShader()->FreeDescriptorSet(_csh);
}

void View::Update()
{
	VERUS_QREF_VM;
	VERUS_QREF_TIMER_GUI;

	// Fade in/out:
	switch (_state)
	{
	case State::fadeIn:
	{
		if (_fade.GetValue() == 0)
			_state = State::active;
	}
	break;
	case State::fadeOut:
	{
		if (_fade.GetValue() == 1)
			_state = State::done;
	}
	break;
	}

	_fade.UpdateClamped(dt); // Call this after switch (_state) to finish drawing black frame.

	// MouseEnter, MouseLeave detection:
	if (State::active == _state && vm.GetViewByZ(0) == this)
	{
		RcCursor cursor = vm.GetCursor();
		PWidget pHovered = _cursor ? GetHovered(cursor.GetX(), cursor.GetY()) : nullptr;
		if (_pLastHovered != pHovered)
		{
			if (_pLastHovered)
				_pLastHovered->InvokeOnMouseLeave();
			if (pHovered)
				pHovered->InvokeOnMouseEnter();
		}
		_pLastHovered = pHovered;
	}

	if (State::done != _state)
	{
		Widget::Update();
		UpdateWidgets();
	}
}

void View::Draw()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_VM;

	auto cb = renderer.GetCommandBuffer();
	auto shader = vm.GetShader();
	auto& ubGui = vm.GetUbGui();
	auto& ubGuiFS = vm.GetUbGuiFS();

	if (IsHidden())
		return;

	if (_tex)
	{
		if (!_csh.IsSet() && _tex->GetTex()->IsLoaded())
			_csh = shader->BindDescriptorSetTextures(1, { _tex->GetTex() });

		if (_csh.IsSet())
		{
			const float ps = GetAnimator().GetPulseScale() + GetAnimator().GetPulseScaleAdd();
			const float pb = (1 - ps) * 0.5f;

			ubGui._matW = Math::QuadMatrix(pb, pb, ps, ps).UniformBufferFormat();
			ubGui._matV = Math::ToUVMatrix(0, 0).UniformBufferFormat();
			ubGui._tcScaleBias = Vector4(1, 1, 0, 0).GLM();
			ubGui._tcMaskScaleBias = Vector4(1, 1, 0, 0).GLM();
			ubGuiFS._color = Vector4::Replicate(1).GLM();

			vm.BindPipeline(ViewManager::PIPE_MAIN, cb);
			shader->BeginBindDescriptors();
			cb->BindDescriptors(shader, 0);
			cb->BindDescriptors(shader, 1, _csh);
			shader->EndBindDescriptors();
			renderer.DrawQuad(cb.Get());
		}
	}
	else if (!GetColor().IsZero())
	{
		ubGui._matW = Math::QuadMatrix().UniformBufferFormat();
		ubGui._matV = Math::ToUVMatrix(0, 0).UniformBufferFormat();
		ubGui._tcScaleBias = Vector4(1, 1, 0, 0).GLM();
		ubGui._tcMaskScaleBias = Vector4(1, 1, 0, 0).GLM();
		ubGuiFS._color = GetColor().GLM();

		vm.BindPipeline(ViewManager::PIPE_SOLID_COLOR, cb);
		shader->BeginBindDescriptors();
		cb->BindDescriptors(shader, 0);
		cb->BindDescriptors(shader, 1, vm.GetDefaultComplexSetHandle());
		shader->EndBindDescriptors();
		renderer.DrawQuad(cb.Get());
	}

	DrawWidgets();

	if (_state == State::fadeIn || _state == State::fadeOut || _state == State::done)
	{
		vm.GetUbGui()._matW = Transform3::UniformBufferFormatIdentity();
		vm.GetUbGuiFS()._color = Vector4(0, 0, 0, _fade.GetValue()).GLM();

		vm.BindPipeline(ViewManager::PIPE_SOLID_COLOR, cb);
		shader->BeginBindDescriptors();
		cb->BindDescriptors(shader, 0);
		cb->BindDescriptors(shader, 1, vm.GetDefaultComplexSetHandle());
		shader->EndBindDescriptors();
		renderer.DrawQuad(cb.Get());
	}
}

void View::Parse(pugi::xml_node node)
{
	SetColor(Vector4(0)); // Default background color is zero.
	Widget::Parse(node);
	ResetOwnerView();

	_cursor = node.attribute("cursor").as_bool(_cursor);
	_debug = node.attribute("debug").as_bool(_debug);
	_fadeSpeed = node.attribute("fadeSpeed").as_float(_fadeSpeed);

	CSZ bg = node.attribute("bg").value();
	if (strlen(bg) > 0)
		_tex.Init(bg);

	ParseWidgets(node, nullptr);

	for (auto stringNode : node.children("string"))
	{
		const int id = stringNode.attribute("id").as_int(-1);
		VERUS_RT_ASSERT(id >= 0);
		String s, textAttr = String("text") + _locale;
		if (auto attr = stringNode.attribute(_C(textAttr)))
			s = attr.value();
		else if (auto attr = stringNode.attribute("text"))
			s = attr.value();
		_mapStrings[id] = s;
	}
}

void View::ResetAnimators(float reverseTime)
{
	GetAnimator().Reset(reverseTime);
	Container::ResetAnimators(reverseTime);
}

void View::OnClick() // Gets called by ViewManager::Update().
{
	if (_state != State::active || IsDisabled())
		return;

	if (!_cursor)
		return;

	VERUS_QREF_VM;

	RcCursor cursor = vm.GetCursor();
	Widget* pHovered = GetHovered(cursor.GetX(), cursor.GetY());
	if (pHovered)
	{
		_pInputFocus = pHovered->AsInputFocus();
		if (_pInputFocus)
			_pInputFocus->InputFocus_OnFocus();
		pHovered->InvokeOnClick(cursor.GetX(), cursor.GetY());
	}
	else
	{
		_pInputFocus = nullptr;
	}
}

void View::OnDoubleClick() // Gets called by ViewManager::Update().
{
	if (_state != State::active || IsDisabled())
		return;

	if (!_cursor)
		return;

	VERUS_QREF_VM;

	RcCursor cursor = vm.GetCursor();
	Widget* pHovered = GetHovered(cursor.GetX(), cursor.GetY());
	if (pHovered)
	{
		_pInputFocus = pHovered->AsInputFocus();
		if (_pInputFocus)
			_pInputFocus->InputFocus_OnFocus();
		if (!pHovered->InvokeOnDoubleClick(cursor.GetX(), cursor.GetY()))
			pHovered->InvokeOnClick(cursor.GetX(), cursor.GetY());
	}
	else
	{
		_pInputFocus = nullptr;
	}
}

void View::OnKey(int scancode)
{
	if (_state != State::active || IsDisabled())
		return;

	const bool handled = InvokeOnKey(scancode);
#ifdef _DEBUG
	if (handled && scancode == SDL_SCANCODE_F5)
		return; // Reloaded, this pointer is invalid.
#endif

	if (_skipNextKey)
	{
		_skipNextKey = false;
		return;
	}

	if (_pInputFocus)
	{
		if (_pInputFocus->InputFocus_KeyOnly())
		{
			_pInputFocus->InputFocus_Key(scancode);
		}
		else
		{
			switch (scancode)
			{
			case SDL_SCANCODE_RETURN:    _pInputFocus->InputFocus_Enter(); _pInputFocus = nullptr; break;
			case SDL_SCANCODE_BACKSPACE: _pInputFocus->InputFocus_BackspaceChar(); break;
			case SDL_SCANCODE_TAB:       _pInputFocus->InputFocus_Tab(); break;
			default:                     _pInputFocus->InputFocus_Key(scancode);
			}
		}
	}
}

void View::OnChar(wchar_t c) // Gets called by ViewManager::OnChar().
{
	if (_state != State::active || IsDisabled())
		return;

	InvokeOnChar(c);

	if (_skipNextKey)
	{
		_skipNextKey = false;
		return;
	}

	if (_pInputFocus && c >= 32)
		_pInputFocus->InputFocus_AddChar(c);
}

CSZ View::GetString(int id) const
{
	VERUS_IF_FOUND_IN(TMapStrings, _mapStrings, id, it)
		return _C(it->second);
	return nullptr;
}

void View::BeginFadeIn() // Gets called by ViewManager::Update().
{
	_state = State::fadeIn;
	_fade.Set(1, -_fadeSpeed);
}

void View::BeginFadeOut()
{
	_state = State::fadeOut;
	_fade.Set(0, _fadeSpeed);

	if (_pLastHovered)
		_pLastHovered->InvokeOnMouseLeave();
	_pLastHovered = nullptr;
}

void View::SetFocus(PInputFocus p)
{
	_pInputFocus = p;
	if (_pInputFocus)
		_pInputFocus->InputFocus_OnFocus();
}
