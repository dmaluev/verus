// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class Cutscene : public Mechanics
		{
		public:
			enum class Fade : int
			{
				none,
				black,
				count
			};

			class Event
			{
			public:
				enum class Type : int
				{
					camera,
					input,
					motion,
					sound,
					sync,
					tr,
					count
				};

			private:
				Type _type = Type::camera;
			};
			VERUS_TYPEDEFS(Event);

		private:
			Vector<Event>     _vEvents;
			Scene::MainCamera _camera;
			Fade              _fadeIn = Fade::none;
			Fade              _fadeOut = Fade::none;
			bool              _interactive = false;

		public:
			Cutscene();
			~Cutscene();

			virtual Continue OnMouseMove(float x, float y) override;
			virtual Continue HandleInput() override;
			virtual Continue Update() override;
			virtual Scene::PMainCamera GetMainCamera() override;
		};
		VERUS_TYPEDEFS(Cutscene);
	}
}
