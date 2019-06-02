#pragma once

#define VERUS_QREF_ASYNC          IO::RAsync              async    = IO::Async::I()
#define VERUS_QREF_ASYS           Audio::RAudioSystem     asys     = Audio::AudioSystem::I()
#define VERUS_QREF_ATMO           Scene::RAtmosphere      atmo     = Scene::Atmosphere::I()
#define VERUS_QREF_BLOOM          Effects::RBloom         bloom    = Effects::Bloom::I()
#define VERUS_QREF_BLUR           Effects::RBlur          blur     = Effects::Blur::I()
#define VERUS_QREF_BULLET         Physics::RBullet        bullet   = Physics::Bullet::I()
#define VERUS_QREF_CONST_SETTINGS App::RcSettings         settings = App::Settings::IConst()
#define VERUS_QREF_DEPTH          Effects::RDepth         depth    = Effects::Depth::I()
#define VERUS_QREF_DR             CGL::RDebugRender       dr       = CGL::DebugRender::I()
#define VERUS_QREF_FSYS           IO::RFileSys            fsys     = IO::FileSys::I()
#define VERUS_QREF_GAME           Game::RGame             game     = Game::Game::I()
#define VERUS_QREF_GRASS          Scene::RGrass           grass    = Scene::Grass::I()
#define VERUS_QREF_GUI            GUI::RGUI               gui      = GUI::GUI::I()
#define VERUS_QREF_HELPERS        Scene::RHelpers         helpers  = Scene::Helpers::I()
#define VERUS_QREF_KM             Input::RKeyMapper       km       = Input::KeyMapper::I()
#define VERUS_QREF_LS             Scene::RLandscape       ls       = Scene::Landscape::I()
#define VERUS_QREF_MM             Scene::RMaterialManager mm       = Scene::MaterialManager::I()
#define VERUS_QREF_MP             Net::RMultiplayer       mp       = Net::Multiplayer::I()
#define VERUS_QREF_PHYSICS        Physics::RPhysics       physics  = Physics::Physics::I()
#define VERUS_QREF_RENDERER       CGI::RRenderer          renderer = CGI::Renderer::I()
#define VERUS_QREF_SETTINGS       App::RSettings          settings = App::Settings::I()
#define VERUS_QREF_SM             Scene::RSceneManager    sm       = Scene::SceneManager::I()
#define VERUS_QREF_SSAO           Effects::RSsao          ssao     = Effects::Ssao::I()
#define VERUS_QREF_TIMER          RTimer                  timer    = Timer::I(); const float dt = timer.GetDeltaTime()
#define VERUS_QREF_TIMER_GUI      RTimer                  timer    = Timer::I(); const float dt = timer.GetDeltaTime(Timer::Type::gui)
#define VERUS_QREF_UTILS          RUtils                  utils    = Utils::I()
#define VERUS_QREF_WATER          Scene::RWater           water    = Scene::Water::I()