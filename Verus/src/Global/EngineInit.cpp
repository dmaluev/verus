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
	if (_makeScene)
		Make_Scene();
	if (_makeGUI)
		Make_GUI();
}

void EngineInit::Free()
{
	if (_makeGUI)
		Free_GUI();
	if (_makeScene)
		Free_Scene();
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

void EngineInit::Init(Input::PKeyMapperDelegate pKeyMapperDelegate, CGI::RendererDelegate* pRendererDelegate)
{
	Timer::I().Init();

	if (_makeIO)
		IO::Async::I().Init();

	// "A.P.I.":
	if (_makeAudio)
		Audio::AudioSystem::I().Init();
	if (_makePhysics)
		Physics::Bullet::I().Init();
	if (_makeInput)
	{
		Input::KeyMapper::I().Init();
		Input::KeyMapper::I().SetDelegate(pKeyMapperDelegate);
	}

	if (_makeCGI)
	{
		CGI::Renderer::I().Init(pRendererDelegate);
	}

	// Static init:
	if (_makeEffects)
		Effects::Particles::InitStatic();
	if (_makeGUI)
		GUI::Font::InitStatic();
	if (_makeScene)
	{
		Scene::Mesh::InitStatic();
		Scene::Terrain::InitStatic();
		Scene::Grass::InitStatic();
		Scene::Forest::InitStatic();
	}

	// Helpers:
	if (_makeCGI)
		CGI::DebugDraw::I().Init();
	if (_makeScene)
		Scene::Helpers::I().Init();

	// Effects:
	if (_makeEffects)
	{
		Effects::Bloom::I().Init();
		Effects::Ssao::I().Init();
		Effects::Blur::I().Init();
	}

	// Materials & textures:
	if (_makeScene)
		Scene::MaterialManager::I().Init();

	if (_makeGUI)
		GUI::ViewManager::I().Init();
}

void EngineInit::InitCmd()
{
	if (_makeEffects)
	{
		Effects::Ssao::I().InitCmd();
	}
}
