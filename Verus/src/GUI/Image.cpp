// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Image::Image()
{
}

Image::~Image()
{
	ViewManager::I().GetShader()->FreeDescriptorSet(_csh);
}

PWidget Image::Make()
{
	return new Image;
}

void Image::Update()
{
	Widget::Update();

	VERUS_QREF_VM;

	if (!_solidColor && !_csh.IsSet() && _tex->GetTex()->IsLoaded())
		_csh = vm.GetShader()->BindDescriptorSetTextures(1, { _tex->GetTex() });

	VERUS_QREF_TIMER_GUI;
	_tcBias.UpdateUnlimited(dt);
	_tcBias.WrapFloatV();
}

void Image::Draw()
{
	if (!GetColor().getW())
		Widget::DrawDebug();

	if (!GetColor().getW())
		return;
	if (!_solidColor && !_csh.IsSet())
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_VM;

	const float pulseScale = GetAnimator().GetPulseScale();
	const float angle = GetAnimator().GetAngle();
	const float postAngle = GetAnimator().GetPostAngle();
	const Vector3 scale(GetWScale() * pulseScale, GetHScale() * pulseScale);
	const Matrix3 matPreR = Matrix3::rotationZ(angle);
	const Matrix3 matPostR = Matrix3::rotationZ(postAngle);

	float x, y;
	GetAbsolutePosition(x, y);
	const Transform3 matW = Transform3(matPostR, Vector3(0)) *
		VMath::appendScale(Math::QuadMatrix(x, y, GetW(), GetH()) * Transform3(matPreR, Vector3(0)), scale);

	auto cb = renderer.GetCommandBuffer();
	auto shader = vm.GetShader();
	auto& ubGui = vm.GetUbGui();
	auto& ubGuiFS = vm.GetUbGuiFS();

	ubGui._matW = matW.UniformBufferFormat();
	ubGuiFS._color = GetColor().GLM();
	if (!_solidColor)
	{
		const Transform3 matV = Math::ToUVMatrix(0, 0);
		ubGui._matV = matV.UniformBufferFormat();
		ubGui._tcScaleBias.x = _tcScale.getX();
		ubGui._tcScaleBias.y = _tcScale.getY();
		RcVector4 bias = _tcBias.GetValue();
		float ub = bias.getX();
		float vb = bias.getY();
		GetAnimator().GetVideoBias(ub, vb);
		ubGui._tcScaleBias.z = ub;
		ubGui._tcScaleBias.w = vb;
	}

	if (_useMask)
	{
		ubGui._tcMaskScaleBias.x = _tcScaleMask.getX();
		ubGui._tcMaskScaleBias.y = _tcScaleMask.getY();
		ubGui._tcMaskScaleBias.z = _tcBiasMask.getX();
		ubGui._tcMaskScaleBias.w = _tcBiasMask.getY();
		vm.BindPipeline(_add ? ViewManager::PIPE_MASK_ADD : ViewManager::PIPE_MASK, cb);
	}
	else if (_solidColor)
	{
		vm.BindPipeline(ViewManager::PIPE_SOLID_COLOR, cb);
	}
	else
	{
		vm.BindPipeline(_add ? ViewManager::PIPE_MAIN_ADD : ViewManager::PIPE_MAIN, cb);
	}

	shader->BeginBindDescriptors();
	cb->BindDescriptors(shader, 0);
	cb->BindDescriptors(shader, 1, _solidColor ? vm.GetDefaultComplexSetHandle() : _csh);
	shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());
}

void Image::Parse(pugi::xml_node node)
{
	Widget::Parse(node);

	String urlAttr("url");
	if (GetOwnerView())
		urlAttr += _C(GetOwnerView()->GetLocale());
	CSZ url = nullptr;
	if (auto attr = node.attribute(_C(urlAttr)))
		url = attr.value();
	else if (auto attr = node.attribute("url"))
		url = attr.value();
	VERUS_RT_ASSERT(url);

	if (strchr(url, ':'))
	{
		_tex.Init(url);
	}
	else
	{
		_solidColor = true;
		Vector4 sc;
		sc.FromColorString(url);
		SetColor(sc);
	}

	_tcScale.setX(ViewManager::ParseCoordX(node.attribute("us").value(), 1));
	_tcScale.setY(ViewManager::ParseCoordY(node.attribute("vs").value(), 1));
	_tcBias.GetValue().setX(ViewManager::ParseCoordX(node.attribute("ub").value()));
	_tcBias.GetValue().setY(ViewManager::ParseCoordY(node.attribute("vb").value()));
	_tcBias.GetSpeed().setX(ViewManager::ParseCoordX(node.attribute("ubSpeed").value()));
	_tcBias.GetSpeed().setY(ViewManager::ParseCoordY(node.attribute("vbSpeed").value()));

	_useMask = node.attribute("useMask").as_bool();
	_add = node.attribute("add").as_bool();

	if (_useMask)
	{
		_tcScaleMask.setX(ViewManager::ParseCoordX(node.attribute("usMask").value(), 1));
		_tcScaleMask.setY(ViewManager::ParseCoordY(node.attribute("vsMask").value(), 1));
		_tcBiasMask.setX(ViewManager::ParseCoordX(node.attribute("ubMask").value()));
		_tcBiasMask.setY(ViewManager::ParseCoordY(node.attribute("vbMask").value()));
	}
}
