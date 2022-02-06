// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Sizer::Sizer()
{
}

Sizer::~Sizer()
{
}

PWidget Sizer::Make()
{
	return new Sizer;
}

void Sizer::Update()
{
	UpdateWidgets();
}

void Sizer::Draw()
{
	DrawWidgets();
}

void Sizer::Parse(pugi::xml_node node)
{
	Widget::Parse(node);

	_cols = node.attribute("cols").as_int(_cols);
	_rows = node.attribute("rows").as_int(_rows);
	_rowHeight = GetH() / float(_rows);
	_colWidth = GetW() / float(_cols);
	ParseWidgets(node, _C(GetID()));
}

void Sizer::GetWidgetAbsolutePosition(PWidget pWidget, float& x, float& y)
{
	const int index = GetWidgetIndex(pWidget);
	if (index < 0)
	{
		x = GetX();
		y = GetY();
	}
	else
	{
		const int offset = GetWidgetCount() - index - 1; // Reverse the z-order.
		const int xOffset = offset % _cols;
		const int yOffset = offset / _cols;
		x = GetX() + _colWidth * xOffset;
		y = GetY() + _rowHeight * yOffset;
	}
}
