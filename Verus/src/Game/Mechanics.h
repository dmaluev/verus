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
			virtual ~Mechanics();

			virtual void OnBegin() {}
			virtual void OnEnd() {}

			virtual Continue HandleInput() { return Continue::yes; }
			virtual Continue Update() { return Continue::yes; }
			virtual Continue Draw() { return Continue::yes; }

			virtual void ApplyReport(const void* pReport) {}
			virtual Continue GetBotDomainCenter(int id, RPoint3 center) { return Continue::yes; }
			virtual Continue OnMouseMove(float x, float y) { return Continue::yes; }
			virtual Continue UpdateMultiplayer() { return Continue::yes; }
			virtual Scene::PMainCamera GetMainCamera() { return nullptr; }
		};
		VERUS_TYPEDEFS(Mechanics);
	}
}
