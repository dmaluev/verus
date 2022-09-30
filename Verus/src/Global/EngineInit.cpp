// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

void EngineInit::Make()
{
	if (_makeGlobal)
		Make_Global();
	if (_makeNet)
		Make_Net();
	if (_makeIO)
		Make_IO();
	if (_makeInput)
		Make_Input();
	if (_makeAudio)
		Make_Audio();
	if (_makeCGI)
		Make_CGI();
	if (_makePhysics)
		Make_Physics();
	if (_makeEffects)
		Make_Effects();
	if (_makeExtra)
		Make_Extra();
	if (_makeWorld)
		Make_World();
	if (_makeGUI)
		Make_GUI();
}

void EngineInit::Free()
{
	// Some object must outlive others, so the order is important:
	if (_makeGUI)
		Free_GUI();
	if (_makeWorld)
		Free_World();
	if (_makeExtra)
		Free_Extra();
	if (_makeEffects)
		Free_Effects();
	if (_makePhysics)
		Free_Physics();
	if (_makeCGI)
		Free_CGI();
	if (_makeAudio)
		Free_Audio();
	if (_makeInput)
		Free_Input();
	if (_makeIO)
		Free_IO();
	if (_makeNet)
		Free_Net();
	if (_makeGlobal)
		Free_Global();
}

void EngineInit::Init(CGI::RendererDelegate* pRendererDelegate)
{
	Timer::I().Init();

	if (_makeIO)
		IO::Async::I().Init();

	if (_makeCGI)
		CGI::Renderer::I().Init(pRendererDelegate, _allowInitShaders);

	// "A.P.I.":
	if (_makeAudio)
		Audio::AudioSystem::I().Init();
	if (_makePhysics)
		Physics::Bullet::I().Init();
	if (_makeInput)
		Input::InputManager::I().Init();

	// Static init:
	if (_makeEffects)
		Effects::Particles::InitStatic();
	if (_makeGUI)
		GUI::Font::InitStatic();
	if (_makeWorld)
	{
		World::Mesh::InitStatic();
		World::Terrain::InitStatic();
		World::Grass::InitStatic();
		World::Forest::InitStatic();
	}

	// Helpers:
	if (_makeCGI && _allowInitShaders)
		CGI::DebugDraw::I().Init();
	if (_makeWorld)
	{
		World::WorldUtils::I().Init();
		World::EditorOverlays::I().Init();
	}

	// Effects:
	if (_makeEffects)
	{
		Effects::Bloom::I().Init();
		Effects::Cinema::I().Init();
		Effects::Ssao::I().Init();
		Effects::Ssr::I().Init();
		Effects::Blur::I().Init();
	}

	// Materials & textures:
	if (_makeWorld)
		World::MaterialManager::I().Init();

	if (_makeGUI)
		GUI::ViewManager::I().Init();
}

void EngineInit::InitCmd()
{
	if (_makeEffects)
		Effects::Ssao::I().InitCmd();
}

void EngineInit::ReducedFeatureSet()
{
	_makeAudio = false;
	_makeEffects = false;
	_makeExtra = false;
	_makeGUI = false;
	_makeNet = false;
	_makePhysics = false;
	_makeWorld = false;
	_allowInitShaders = false;
}
