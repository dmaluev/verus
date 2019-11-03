#pragma once

namespace verus
{
	class Parallel
	{
	public:
		template<typename TFunc>
		static void For(int from, int to, TFunc func, int minTime = 0, int minShare = 1)
		{
			const int total = to - from;
			VERUS_RT_ASSERT(minShare <= total);
			const UINT32 numCoresLimit = total / minShare;
			const int numCores = Math::Min(std::thread::hardware_concurrency(), numCoresLimit);
			const int numExThreads = numCores - 1;
			const int share = total / numCores;
			const int extra = total - share * numExThreads;
			std::vector<std::thread> v;
			v.reserve(numExThreads);
			const std::chrono::high_resolution_clock::time_point t0 = std::chrono::high_resolution_clock::now();
			VERUS_FOR(i, numExThreads)
			{
				v.push_back(std::thread([i, share, func]()
					{
						const int from = share * i;
						const int to = from + share;
						for (int j = from; j < to; ++j)
							func(j);
					}));
			}
			{
				const int from = share * numExThreads;
				const int to = from + extra;
				for (int j = from; j < to; ++j)
					func(j);
			}
			VERUS_FOREACH(std::vector<std::thread>, v, it)
				it->join();
			const std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
			const std::chrono::milliseconds d = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
			VERUS_RT_ASSERT(!minTime || d.count() >= minTime);
		}
	};
}
