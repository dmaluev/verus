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
	VERUS_DONE(Cursor);
}

void Cursor::Draw()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_VM;
	//VERUS_QREF_RENDER;
	//
	//gui.GetStateBlockAlphaBlend()->Apply();
	//
	//const CVector3 scale(512 / 1920.f, 512 / 1080.f);
	//
	//render->SetTextures({ _tex->GetTex(), _tex->GetTex() });
	//gui.GetCbPerObject().matW = Math::QuadMatrix(m_x - _hotspotX, _y - _hotspotY, scale.getX(), scale.getY()).ConstBufferFormat();
	//gui.GetCbPerObject().matV = Math::ToUVMatrix(0, 0).ConstBufferFormat();
	//gui.GetCbPerObject().colorAmbient = Vector4(1, 1, 1, 1);
	//gui.GetCbPerObject().tcScaleBias = Vector4(1, 1, 0, 0);
	//gui.GetShader()->Bind("T");
	//gui.GetShader()->UpdateBuffer(0);
	//render.DrawQuad();
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
	_x = Math::Clamp<float>(_x + x * (100.f / settings._displaySizeWidth), 0, 1);
	_y = Math::Clamp<float>(_y + y * (100.f / settings._displaySizeHeight), 0, 1);
}
