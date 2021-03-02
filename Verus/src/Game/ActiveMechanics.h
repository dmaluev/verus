// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class ActiveMechanics : public Object
		{
			Vector<PMechanics> _vStack;

		public:
			ActiveMechanics();
			~ActiveMechanics();

			void Init();
			void Done();

			bool HandleInput();
			bool Update();
			bool Draw();

			void ApplyReport(const void* pReport);
			bool GetBotDomainCenter(int id, RPoint3 center);
			bool OnMouseMove(float x, float y);
			bool UpdateMultiplayer();
			Scene::PMainCamera GetMainCamera();

			void Reset();
			bool Push(RMechanics mech);
			bool Pop(RMechanics mech);
			bool IsActive(RcMechanics mech) const;
			virtual void ActiveMechanics_OnChanged() {}
		};
		VERUS_TYPEDEFS(ActiveMechanics);
	}
}
