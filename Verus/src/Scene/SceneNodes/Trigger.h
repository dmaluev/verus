// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class Trigger : public SceneNode
		{
		public:
			struct Desc
			{
				pugi::xml_node _node;
			};
			VERUS_TYPEDEFS(Desc);

			Trigger();
			~Trigger();

			void Init(RcDesc desc);
			void Done();
		};
		VERUS_TYPEDEFS(Trigger);

		class TriggerPtr : public Ptr<Trigger>
		{
		public:
			void Init(Trigger::RcDesc desc);
		};
		VERUS_TYPEDEFS(TriggerPtr);

		class TriggerPwn : public TriggerPtr
		{
		public:
			~TriggerPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(TriggerPwn);
	}
}
