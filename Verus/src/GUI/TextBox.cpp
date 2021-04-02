// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

TextBox::TextBox()
{
}

TextBox::~TextBox()
{
}

PWidget TextBox::Make()
{
	return new TextBox;
}

void TextBox::Update()
{
	Label::Update();

	if (_blinkCooldown.IsFinished())
	{
		_blinkCooldown.Start();
		_blinkState = !_blinkState;
	}
}

void TextBox::Draw()
{
	VERUS_QREF_VM;
	VERUS_QREF_RENDERER;

	Widget::DrawInputStyle();
	Label::Draw();

	// Draw caret:
	if (_blinkState && GetOwnerView()->IsInFocus(this))
	{
		float x, y;
		GetAbsolutePosition(x, y);

		PFont pFont = vm.FindFont(GetFont());
		const float caretX = Font::ToFloatX(pFont->GetTextWidth(_C(GetText())), GetFontScale());

		auto cb = renderer.GetCommandBuffer();
		auto shader = vm.GetShader();

		vm.GetUbGui()._matW = Math::QuadMatrix(x + caretX, y, 0.0015f, GetH()).UniformBufferFormat();
		vm.GetUbGui()._matV = Math::ToUVMatrix(0, 0).UniformBufferFormat();
		vm.GetUbGuiFS()._color = Vector4(1, 1, 1, 1).GLM();

		vm.BindPipeline(ViewManager::PIPE_SOLID_COLOR, cb);
		shader->BeginBindDescriptors();
		cb->BindDescriptors(shader, 0);
		cb->BindDescriptors(shader, 1, vm.GetDefaultComplexSetHandle());
		shader->EndBindDescriptors();
		renderer.DrawQuad(cb.Get());
	}
}

void TextBox::Parse(pugi::xml_node node)
{
	Label::Parse(node);

	_fullText = _C(GetText());
	_cursor = Utils::Cast32(GetText().Length());

	_maxLength = node.attribute("maxLength").as_int(_maxLength);

	if (auto attr = node.attribute("ruleLower"))
		_rule |= (attr.as_int() > 0) ? Rule::lower : Rule::none;
	if (auto attr = node.attribute("ruleUpper"))
		_rule |= (attr.as_int() > 0) ? Rule::upper : Rule::none;
	if (auto attr = node.attribute("ruleNumber"))
		_rule |= (attr.as_int() > 0) ? Rule::number : Rule::none;
}

void TextBox::Focus()
{
	GetOwnerView()->SetFocus(this);
}

void TextBox::InputFocus_AddChar(wchar_t c)
{
	if (_maxLength > 0 && static_cast<int>(_fullText.length()) >= _maxLength)
		return;

	if (_rule & Rule::lower)
		c = tolower(c);
	if (_rule & Rule::upper)
		c = toupper(c);
	if (_rule & Rule::number)
	{
		if (c < '0' || c > '9')
			return;
	}

	_fullText.insert(_cursor, 1, c);
	_cursor++;
	UpdateLabelText();

	InvokeOnChar(c);
}

void TextBox::InputFocus_DeleteChar()
{
	if (_cursor < _fullText.length())
	{
		_fullText.erase(_cursor);
		UpdateLabelText();

		InvokeOnChar(0);
	}
}

void TextBox::InputFocus_BackspaceChar()
{
	if (_cursor > 0)
	{
		_fullText.erase(_cursor - 1, 1);
		_cursor--;
		UpdateLabelText();

		InvokeOnChar(0);
	}
}

void TextBox::InputFocus_SetCursor(int pos)
{
	pos = Math::Clamp(pos, 0, static_cast<int>(_fullText.length()));
	_cursor = pos;
	UpdateLabelText();
}

void TextBox::InputFocus_MoveCursor(int delta)
{
	InputFocus_SetCursor(_cursor + delta);
}

void TextBox::InputFocus_Enter()
{
}

void TextBox::InputFocus_Tab()
{
	InvokeOnChar(L'\t');
}

void TextBox::InputFocus_Key(int scancode)
{
	InvokeOnKey(scancode);
}

void TextBox::InputFocus_OnFocus()
{
	_blinkCooldown.Start();
	_blinkState = true;
}

void TextBox::UpdateLabelText()
{
	Label::SetText(_C(_fullText.substr(_viewOffset)));
}

WideStr TextBox::GetText() const
{
	return _C(_fullText);
}

void TextBox::SetText(CWSZ txt)
{
	if (!txt)
		return;
	_fullText = txt;
	_cursor = Utils::Cast32(_fullText.length());
	UpdateLabelText();
}

void TextBox::SetText(CSZ txt)
{
	if (!txt)
		return;
	_fullText = Str::Utf8ToWide(txt);
	_cursor = Utils::Cast32(_fullText.length());
	UpdateLabelText();
}
