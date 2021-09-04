// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class Cutscene : public Object, public Mechanics
		{
		public:
			class Command
			{
			protected:
				Cutscene* _pCutscene = nullptr;
				String    _url;
				float     _delay = 0;
				float     _prevDelay = FLT_MAX;
				float     _endDelay = FLT_MAX;
				float     _duration = 0;
				bool      _active = false;

			public:
				virtual ~Command() {}
				virtual void Parse(pugi::xml_node node);
				virtual bool Update();
				virtual bool DrawOverlay() { return false; }
				virtual void OnBegin() { _active = true; }
				virtual void OnEnd() { _active = false; }
				virtual bool IsBarrier() const { return false; }
				virtual bool SkipHere() const { return false; }

				void SetCutscene(Cutscene* p) { _pCutscene = p; }
				bool IsActive() const { return _active; }
				bool IsDelayed() const { return _pCutscene->_timeSinceBarrier < _delay; }
				float GetDelay() const { return _delay; }
				void AdjustDelayBy(float amount) { _delay += amount; }
				void UpdatePrevDelay(float newActiveDuration);
				void UpdateEndDelay(float newActiveDuration);
				float GetDuration() const { return _duration; }
				void LimitActiveDuration(float limit);
				float GetTime() const { return _pCutscene->_timeSinceBarrier - _delay; }
			};
			VERUS_TYPEDEFS(Command);

			class BarrierCommand : public Command
			{
			public:
				static PCommand Make();
				virtual bool IsBarrier() const override { return true; }
			};
			VERUS_TYPEDEFS(BarrierCommand);

			class CameraCommand : public Command, public IO::AsyncDelegate, public Anim::MotionDelegate
			{
				Anim::Motion _motion;
				String       _cameraName;

			public:
				~CameraCommand();
				static PCommand Make();
				virtual void Parse(pugi::xml_node node) override;
				virtual bool Update() override;
				virtual void OnBegin() override;
				virtual void OnEnd() override;
				virtual void Async_WhenLoaded(CSZ url, RcBlob blob) override;
				virtual void Motion_OnTrigger(CSZ name, int state) override;
			};
			VERUS_TYPEDEFS(CameraCommand);

			class ConfigCommand : public Command
			{
				int  _tag = 1;
				bool _callOnEnd = false;
				bool _skipHere = false;

			public:
				static PCommand Make();
				virtual void Parse(pugi::xml_node node) override;
				virtual void OnBegin() override;
				virtual bool SkipHere() const override { return _skipHere; }
			};
			VERUS_TYPEDEFS(ConfigCommand);

			class FadeCommand : public Command
			{
				Vector4 _from = Vector4(0);
				Vector4 _to = Vector4(0);
				Easing  _easing = Easing::none;
				bool    _skipHere = false;

			public:
				static PCommand Make();
				virtual void Parse(pugi::xml_node node) override;
				virtual bool DrawOverlay() override;
				virtual bool SkipHere() const override { return _skipHere; }
			};
			VERUS_TYPEDEFS(FadeCommand);

			class MotionCommand : public Command, public Anim::MotionDelegate
			{
				Anim::Motion _motion;

			public:
				static PCommand Make();
				virtual bool Update() override;
				virtual void Motion_OnTrigger(CSZ name, int state) override;
			};
			VERUS_TYPEDEFS(MotionCommand);

			class SoundCommand : public Command
			{
				Audio::SoundPwn  _sound;
				Audio::SourcePtr _source;
				float            _gain = 1;
				float            _pitch = 1;

			public:
				static PCommand Make();
				virtual void Parse(pugi::xml_node node) override;
				virtual bool Update();
				virtual void OnEnd() override;
			};
			VERUS_TYPEDEFS(SoundCommand);

		private:
			typedef PCommand(*PFNCREATOR)();
			typedef Map<String, PFNCREATOR> TMapCreators;

			String                   _url;
			TMapCreators             _mapCreators;
			Vector<PCommand>         _vCommands;
			std::function<void(int)> _fnOnEnd;
			PCameraCommand           _pCurrentCameraCommand = nullptr;
			Scene::MainCamera        _camera;
			int                      _beginIndex = 0;
			int                      _endIndex = 0;
			float                    _timeSinceBarrier = 0;
			bool                     _interactive = false;

		public:
			Cutscene();
			~Cutscene();

			void Init();
			void Done();

			void Next(CSZ url, bool preload = false);
			void Load(CSZ url);

			virtual void OnBegin() override;
			virtual void OnEnd() override;
			virtual bool CanAutoPop() override;

			// Commands:
			void RegisterCommand(CSZ type, PFNCREATOR pCreator);
			PCommand CreateCommand(CSZ type);
			void DeleteCommands();

			// Run commands:
			void Skip();
			bool SkipBarrier();
			bool RunParallelCommands();

			virtual Continue Update() override;
			virtual Continue DrawOverlay() override;
			virtual bool IsInputEnabled() override;
			virtual Continue OnMouseMove(float x, float y) override;
			virtual Scene::PMainCamera GetMainCamera() override;

			void OnWindowSizeChanged();

			void SetOnEndCallback(std::function<void(int)> fn) { _fnOnEnd = fn; }

			void OnNewActiveDuration(float newActiveDuration);
		};
		VERUS_TYPEDEFS(Cutscene);
	}
}
