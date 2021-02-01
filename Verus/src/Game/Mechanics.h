// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class Mechanics
		{
		public:
			Mechanics();
			~Mechanics();

			virtual Continue OnMouseMove(float x, float y) { return Continue::yes; }
			virtual Continue HandleInput() { return Continue::yes; }
			virtual Continue Update() { return Continue::yes; }
			virtual Scene::PMainCamera GetMainCamera() { return nullptr; }
		};
		VERUS_TYPEDEFS(Mechanics);
	}
}
