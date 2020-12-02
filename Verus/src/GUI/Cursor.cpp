// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

Cursor::Cursor()
{
}

Cursor::~Cursor()
{
	Done();
}

void Cursor::Init()
{
	VERUS_INIT();

	_tex.Init("[Textures]:UI/Cursor.dds");
}

void Cursor::Done()
{
	if (ViewManager::I().GetShader())
		ViewManager::I().GetShader()->FreeDescriptorSet(_csh);
	VERUS_DONE(Cursor);
}

void Cursor::Draw()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_VM;

	if (!_csh.IsSet())
	{
		if (_tex->GetTex()->IsLoaded())
			_csh = vm.GetShader()->BindDescriptorSetTextures(1, { _tex->GetTex() });
		else
			return;
	}

	const Vector3 scale(512 / 1920.f, 512 / 1080.f);

	auto cb = renderer.GetCommandBuffer();
	auto shader = vm.GetShader();
	auto& ubGui = vm.GetUbGui();
	auto& ubGuiFS = vm.GetUbGuiFS();

	ubGui._matW = Math::QuadMatrix(_x - _hotspotX, _y - _hotspotY, scale.getX(), scale.getY()).UniformBufferFormat();
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

void Cursor::MoveHotspotTo(float x, float y)
{
	_hotspotX = x;
	_hotspotY = y;
}

void Cursor::MoveBy(float x, float y)
{
	if (!IsInitialized())
		return;

	VERUS_QREF_CONST_SETTINGS;

	const float scale = Game::BaseGame::GetMouseScale();
	const float invScale = 1 / scale;
	_x = Math::Clamp<float>(_x + x * (invScale / settings._displaySizeWidth), 0, 1);
	_y = Math::Clamp<float>(_y + y * (invScale / settings._displaySizeHeight), 0, 1);
}
