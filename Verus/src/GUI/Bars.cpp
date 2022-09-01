// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Bars::Bars()
{
}

Bars::~Bars()
{
}

PWidget Bars::Make()
{
	return new Bars;
}

void Bars::Update()
{
	Widget::Update();
}

void Bars::Draw()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_VM;

	auto pExtReality = renderer->GetExtReality();
	if (pExtReality->IsInitialized())
		return; // No bars in XR.

	const float currentAspectRatio = renderer.GetCurrentViewAspectRatio();
	const float barsRatio = Math::Max(0.f, 1 - (1 / _aspectRatio * currentAspectRatio));
	if (!barsRatio)
		return;

	const float barRatio = barsRatio * 0.5f;
	float x, y;
	GetAbsolutePosition(x, y);

	auto cb = renderer.GetCommandBuffer();
	auto shader = vm.GetShader();
	auto& ubGui = vm.GetUbGui();
	auto& ubGuiFS = vm.GetUbGuiFS();

	ubGuiFS._color = GetColor().GLM();

	vm.BindPipeline(ViewManager::PIPE_SOLID_COLOR, cb);
	shader->BeginBindDescriptors();

	ubGui._matWVP = Math::QuadMatrix(x, y, GetW(), GetH() * barRatio).UniformBufferFormat();
	cb->BindDescriptors(shader, 0);
	cb->BindDescriptors(shader, 1, vm.GetDefaultComplexSetHandle());
	renderer.DrawQuad(cb.Get());

	ubGui._matWVP = Math::QuadMatrix(x, y + GetH() * (1 - barRatio), GetW(), GetH() * barRatio).UniformBufferFormat();
	cb->BindDescriptors(shader, 0);
	cb->BindDescriptors(shader, 1, vm.GetDefaultComplexSetHandle());
	renderer.DrawQuad(cb.Get());

	shader->EndBindDescriptors();
}

void Bars::Parse(pugi::xml_node node)
{
	Widget::Parse(node);
}
