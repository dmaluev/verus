#pragma once

namespace verus
{
	namespace Game
	{
		class BaseGame : public Input::KeyMapperDelegate
		{
			struct Pimpl;
			Pimpl*           _p = nullptr;
			AlignedAllocator _alloc;
			EngineInit       _engineInit;

		public:
			BaseGame();
			~BaseGame();

			void Initialize(VERUS_MAIN_DEFAULT_ARGS);
			void Run();
			void Exit();

			virtual void BaseGame_LoadContent() = 0;
			virtual void BaseGame_UnloadContent() = 0;
			virtual void BaseGame_HandleInput() = 0;
			virtual void BaseGame_Update() = 0;
			virtual void BaseGame_Draw() = 0;
			virtual void BaseGame_DrawOverlay() = 0;
			virtual void BaseGame_OnActivated() {}
			virtual void BaseGame_OnDeactivated() {}
			virtual void BaseGame_OnMouseMove(float x, float y) {}
			virtual void BaseGame_OnKey(int scancode) {}
			virtual void BaseGame_OnChar(wchar_t c) {}

			virtual void KeyMapper_OnMouseMove(int x, int y) override;
			virtual void KeyMapper_OnKey(int scancode) override;
			virtual void KeyMapper_OnChar(wchar_t c) override;

			// Internal objects:
			Scene::RCamera GetCamera();
			//RCharacter GetCameraCharacter();

			// Configurations:
			void ActivateDefaultCameraMovement(bool b);
			void ShowFPS(bool b);
			void DebugBullet(bool b);
			bool IsDebugBulletMode() const;
		};
		VERUS_TYPEDEFS(BaseGame);
	}
}
