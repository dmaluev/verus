// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class Entity : public Object
		{
			int _refCount = 0;

		public:
			Entity();
			~Entity();

			void Init();
			bool Done();

			void AddRef() { _refCount++; }
			int GetRefCount() const { return _refCount; }
		};
		VERUS_TYPEDEFS(Entity);
	}
}
