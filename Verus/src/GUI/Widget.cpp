// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Widget::Widget()
{
}

Widget::~Widget()
{
}

WideStr Widget::GetText() const
{
	return _fixedTextLength ? _vFixedText.data() : _C(_text);
}

void Widget::SetText(CWSZ text)
{
	if (_fixedTextLength)
		wcsncpy(_vFixedText.data(), text, Math::Min<int>(Utils::Cast32(wcslen(text)), _fixedTextLength - 1));
	else
		_text = text;
}

void Widget::SetText(CSZ text)
{
	if (_fixedTextLength)
		Str::Utf8ToWide(text, _vFixedText.data(), _fixedTextLength);
	else
		_text = Str::Utf8ToWide(text);
}

void Widget::DrawDebug()
{
	PView pOwnerView = GetOwnerView();
	if (!pOwnerView->IsDebug())
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_VM;

	float x, y;
	GetAbsolutePosition(x, y);

	auto cb = renderer.GetCommandBuffer();
	auto shader = vm.GetShader();
	auto& ubGui = vm.GetUbGui();
	auto& ubGuiFS = vm.GetUbGuiFS();

	ubGui._matW = Math::QuadMatrix(x, y, GetW(), GetH()).UniformBufferFormat();
	ubGui._matV = Math::ToUVMatrix(0, 0).UniformBufferFormat();
	ubGui._tcScaleBias = Vector4(1, 1, 0, 0).GLM();
	ubGui._tcMaskScaleBias = Vector4(1, 1, 0, 0).GLM();
	ubGuiFS._color = Vector4::Replicate(1).GLM();

	vm.BindPipeline(ViewManager::PIPE_MAIN, cb);
	shader->BeginBindDescriptors();
	cb->BindDescriptors(shader, 0);
	cb->BindDescriptors(shader, 1, vm.GetDebugComplexSetHandle());
	shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());
}

void Widget::DrawInputStyle()
{
	VERUS_QREF_VM;
	VERUS_QREF_RENDERER;

	float x, y;
	GetAbsolutePosition(x, y);

	auto cb = renderer.GetCommandBuffer();
	auto shader = vm.GetShader();

	vm.GetUbGui()._matW = Math::QuadMatrix(x, y, GetW(), GetH()).UniformBufferFormat();
	vm.GetUbGuiFS()._color = Vector4(0, 0, 0, 0.75f * GetColor().getW()).GLM();

	vm.BindPipeline(ViewManager::PIPE_SOLID_COLOR, cb);
	shader->BeginBindDescriptors();
	cb->BindDescriptors(shader, 0);
	cb->BindDescriptors(shader, 1, vm.GetDefaultComplexSetHandle());
	shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());
}

void Widget::Update()
{
	if (_animator.Update())
		InvokeOnTimeout();
}

void Widget::GetRelativePosition(float& x, float& y)
{
	x = GetX();
	y = GetY();
}

void Widget::GetAbsolutePosition(float& x, float& y)
{
	VERUS_QREF_VM;
	PView pView = GetOwnerView();
	if (_sizer.empty())
	{
		float xView = 0, yView = 0;
		if (pView)
			pView->GetAbsolutePosition(xView, yView);
		x = GetX() + xView;
		y = GetY() + yView;
	}
	else
	{
		PSizer pSizer = static_cast<PSizer>(pView->GetWidgetById(_C(_sizer)));
		float xFromSizer = 0, yFromSizer = 0;
		pSizer->GetWidgetAbsolutePosition(this, xFromSizer, yFromSizer);
		x = GetX() + xFromSizer;
		y = GetY() + yFromSizer;
	}
}

void Widget::GetViewport(int xywh[4])
{
	VERUS_QREF_RENDERER;

	float x, y;
	GetAbsolutePosition(x, y);
	xywh[0] = int(x * renderer.GetSwapChainWidth());
	xywh[1] = int(y * renderer.GetSwapChainHeight());
	xywh[2] = int(GetW() * renderer.GetSwapChainWidth());
	xywh[3] = int(GetH() * renderer.GetSwapChainHeight());
}

void Widget::Parse(pugi::xml_node node)
{
	_ownerView = _C(ViewManager::I().GetCurrentParseView()->GetName());

	_id = node.attribute("id").value();

	String textAttr("text");
	if (GetOwnerView())
		textAttr += _C(GetOwnerView()->GetLocale());

	_fixedTextLength = node.attribute("fixedTextLength").as_int();
	if (_fixedTextLength > 0 && _fixedTextLength <= USHRT_MAX)
		_vFixedText.resize(_fixedTextLength);
	else
		_fixedTextLength = 0;

	if (auto attr = node.attribute(_C(textAttr)))
		SetText(attr.value());
	else if (auto attr = node.attribute("text"))
		SetText(attr.value());

	CSZ color = node.attribute("color").value();
	if (strlen(color) > 0)
		_color.FromColorString(color);

	_x = ViewManager::ParseCoordX(node.attribute("x").value());
	_y = ViewManager::ParseCoordY(node.attribute("y").value());
	_w = ViewManager::ParseCoordX(node.attribute("w").value(), 1);
	_h = ViewManager::ParseCoordY(node.attribute("h").value(), 1);

	_hidden = node.attribute("hidden").as_bool();
	_disabled = node.attribute("disabled").as_bool();

	_useAspect = node.attribute("useAspect").as_bool();
	if (_useAspect)
	{
		VERUS_QREF_RENDERER;
		const float newW = _w / renderer.GetSwapChainAspectRatio();
		_aspectOffset = (_w - newW) * 0.5f;
		_w = newW;
		_x += _aspectOffset;
	}

	_animator.Parse(node, this);
}

bool Widget::IsHovered(float x, float y)
{
	if (_disabled)
		return false;

	float xmin, ymin;
	GetAbsolutePosition(xmin, ymin);
	const float xmax = xmin + GetW(), ymax = ymin + GetH();
	return
		(x >= xmin) && (x < xmax) &&
		(y >= ymin) && (y < ymax);
}

PView Widget::GetOwnerView()
{
	VERUS_QREF_VM;
	return vm.GetViewByName(_C(_ownerView));
}

void Widget::ResetOwnerView()
{
	_ownerView.clear();
}

bool Widget::InvokeOnMouseEnter()
{
	if (_fnOnMouseEnter)
	{
		EventArgs args(_C(_ownerView), _C(_id), this);
		_fnOnMouseEnter(args);
		return true;
	}
	return false;
}

bool Widget::InvokeOnMouseLeave()
{
	if (_fnOnMouseLeave)
	{
		EventArgs args(_C(_ownerView), _C(_id), this);
		_fnOnMouseLeave(args);
		return true;
	}
	return false;
}

bool Widget::InvokeOnClick(float x, float y)
{
	if (_fnOnClick)
	{
		EventArgs args(_C(_ownerView), _C(_id), this);
		_fnOnClick(args);
		return true;
	}
	return false;
}

bool Widget::InvokeOnDoubleClick(float x, float y)
{
	if (_fnOnDoubleClick)
	{
		EventArgs args(_C(_ownerView), _C(_id), this);
		_fnOnDoubleClick(args);
		return true;
	}
	return false;
}

bool Widget::InvokeOnKey(int scancode)
{
	if (_fnOnKey)
	{
		EventArgs args(_C(_ownerView), _C(_id), this, scancode);
		_fnOnKey(args);
		return true;
	}
	return false;
}

bool Widget::InvokeOnChar(wchar_t c)
{
	if (_fnOnChar)
	{
		EventArgs args(_C(_ownerView), _C(_id), this, c);
		_fnOnChar(args);
		return true;
	}
	return false;
}

bool Widget::InvokeOnTimeout()
{
	if (_fnOnTimeout)
	{
		EventArgs args(_C(_ownerView), _C(_id), this);
		_fnOnTimeout(args);
		return true;
	}
	return false;
}
