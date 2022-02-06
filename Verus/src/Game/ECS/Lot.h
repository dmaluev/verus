// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		typedef StoreUnique<String, Entity> TStoreEntities;
		class Lot : public Object, private TStoreEntities
		{
			int _refCount = 0;

		public:
			Lot();
			~Lot();

			void Init();
			bool Done();

			void AddRef() { _refCount++; }
			int GetRefCount() const { return _refCount; }

			// Entities:
			PEntity InsertEntity(CSZ uid);
			PEntity FindEntity(CSZ uid);
			void DeleteEntity(CSZ uid);
			void DeleteAllEntities();
			template<typename T>
			void ForEachEntity(const T& fn)
			{
				VERUS_FOREACH_X(TStoreEntities::TMap, TStoreEntities::_map, it)
				{
					auto& entity = *it++;
					if (Continue::no == fn(entity.second))
						return;
				}
			}
		};
		VERUS_TYPEDEFS(Lot);
	}
}
