#include "verus.h"

using namespace verus;

void EngineInit::Make()
{
#if 0
	if (_makeUtils)
		Make_Utils();
	if (_makeNet)
		Make_Net();
	if (_makeIO)
		Make_IO();
	if (_makeInput)
		Make_Input();
	if (_makeAudio)
		Make_Audio();
	if (_makeCGL)
		Make_CGL();
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
#endif
}

void EngineInit::Free()
{
#if 0
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
	if (_makeCGL)
		Free_CGL();
	if (_makeAudio)
		Free_Audio();
	if (_makeInput)
		Free_Input();
	if (_makeIO)
		Free_IO();
	if (_makeNet)
		Free_Net();
	if (_makeUtils)
		Free_Utils();
#endif
}

void EngineInit::Init(Input::PKeyMapperDelegate pKeyMapperDelegate, CGI::RenderDelegate* pRenderDelegate, bool createWindow)
{
#if 0
	if (_makeUtils)
		Utils::CTimer::I().Init();

	if (_makeIO)
		IO::CAsync::I().Init();

	// "A.P.I.":
	if (_makeAudio)
		Audio::CAudio::I().Init();
	if (_makePhysics)
		Physics::CBullet::I().Init();
	if (_makeInput)
	{
		Input::CKeyMapper::I().Init();
		Input::CKeyMapper::I().SetDelegate(pKeyMapperDelegate);
	}

	if (_makeCGL)
	{
		CGL::CRender::I().Init(pRenderDelegate, createWindow);
#ifdef _WIN32
		CGL::CRender::InitWin32(_C(CUtils::I().GetWritablePath()), 101);
#endif
	}

	// Static init:
	if (_makeEffects)
		Effects::CParticles::InitStatic();
	if (_makeGUI)
		GUI::CFont::InitStatic();
	if (_makeScene)
	{
		Scene::CMesh::InitStatic();
		Scene::CTerrain::InitStatic();
		Scene::CForest::InitStatic();
	}

	// Helpers:
	if (_makeCGL)
		CGL::CDebugRender::I().Init();
	if (_makeScene)
		Scene::CHelpers::I().Init();

	// Effects:
	if (_makeEffects)
	{
		Effects::CBlur::I().Init();
		Effects::CBloom::I().Init();
		Effects::CSsao::I().Init();
	}

	// Materials & textures:
	if (_makeScene)
		Scene::CMaterialManager::I().Init();

	if (_makeGUI)
		GUI::CGUI::I().Init();
#endif
}
