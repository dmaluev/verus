// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Button::Button()
{
}

Button::~Button()
{
}

PWidget Button::Make()
{
	return new Button;
}

void Button::Update()
{
	Widget::Update();

	_label.Update();
	_image.Update();

	float x, y;
	GetAbsolutePosition(x, y);
	_label.SetX(x);
	_label.SetY(y + (GetH() - _label.GetFontH()) * 0.5f);
	_image.SetX(x);
	_image.SetY(y);
	_label.SetH(_label.GetFontH());

	if (_hasIcon)
	{
		VERUS_QREF_RENDERER;

		_icon.Update();
		const float iconW = GetH() / renderer.GetCurrentViewAspectRatio();
		_icon.SetX(x);
		_icon.SetY(y);
		_icon.SetW(iconW);
		_icon.SetH(GetH());
		const float textOffset = iconW;
		_label.SetX(x + textOffset);
		_label.SetW(GetW() - textOffset);
	}
}

void Button::Draw()
{
	_image.Draw();
	if (_hasIcon)
		_icon.Draw();
	_label.Draw();
}

void Button::Parse(pugi::xml_node node)
{
	Widget::Parse(node);

	_label.Parse(node);
	_image.Parse(node);
	for (auto imageNode : node.children("image"))
	{
		_hasIcon = true;
		_icon.Parse(imageNode);
	}
}
