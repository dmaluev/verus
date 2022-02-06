// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		typedef StoreUnique<String, Lot> TStoreLots;
		class LotManager : public Singleton<LotManager>, public Object, private TStoreLots
		{
		public:
			LotManager();
			virtual ~LotManager();

			void Init();
			void Done();

			// Lots:
			PLot InsertLot(CSZ uid);
			PLot FindLot(CSZ uid);
			void DeleteLot(CSZ uid);
			void DeleteAllLots();
			template<typename T>
			void ForEachLot(const T& fn)
			{
				VERUS_FOREACH_X(TStoreLots::TMap, TStoreLots::_map, it)
				{
					auto& lot = *it++;
					if (Continue::no == fn(lot.second))
						return;
				}
			}
		};
		VERUS_TYPEDEFS(LotManager);
	}
}
