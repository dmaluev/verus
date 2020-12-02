// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class ChainAward
		{
			float _lastTime = 0;
			float _maxInterval = 0;
			int   _lastLevel = 0;
			int   _maxLevel = 0;

		public:
			ChainAward(float maxInterval = 1, int maxLevel = 10);
			~ChainAward();

			int Score();
			void Reset();
		};
		VERUS_TYPEDEFS(ChainAward);
	}
}
