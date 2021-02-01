// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

Cutscene::Cutscene()
{
}

Cutscene::~Cutscene()
{
}

Continue Cutscene::OnMouseMove(float x, float y)
{
	return _interactive ? Continue::yes : Continue::no;
}

Continue Cutscene::HandleInput()
{
	return _interactive ? Continue::yes : Continue::no;
}

Continue Cutscene::Update()
{
	return Continue::yes;
}

Scene::PMainCamera Cutscene::GetMainCamera()
{
	return _interactive ? nullptr : &_camera;
}
