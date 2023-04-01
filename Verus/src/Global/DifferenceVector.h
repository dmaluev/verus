// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	template<typename T>
	class DifferenceVector
	{
		Vector<T> _v[2];
		int       _ping = 0;
		int       _pong = 1;

	public:
		void Reserve(int capacity)
		{
			_v[0].reserve(capacity);
			_v[1].reserve(capacity);
		}

		void PushBack(const T& x)
		{
			_v[_ping].push_back(x);
		}

		template<typename TFnInsert, typename TFnDelete>
		void HandleDifference(const TFnInsert& fnInsert, const TFnDelete& fnDelete)
		{
			// Sort and remove duplicates:
			std::sort(_v[_ping].begin(), _v[_ping].end(), [](const T& a, const T& b)
				{
					return a.GetID() < b.GetID();
				});
			_v[_ping].erase(
				std::unique(_v[_ping].begin(), _v[_ping].end(), [](const T& a, const T& b) {return a.GetID() == b.GetID(); }),
				_v[_ping].end());

			typename Vector<T>::iterator itPing = _v[_ping].begin(), itPingEnd = _v[_ping].end();
			typename Vector<T>::iterator itPong = _v[_pong].begin(), itPongEnd = _v[_pong].end();
			while (itPing != itPingEnd)
			{
				if (itPong == itPongEnd) // Ran out of pong elements?
				{
					while (itPing != itPingEnd) // Ping leftovers.
						fnInsert(*itPing++);
					break;
				}

				if (itPing->GetID() < itPong->GetID()) // Found new element in ping?
				{
					fnInsert(*itPing++);
				}
				else if (itPong->GetID() < itPing->GetID()) // Found outdated element in pong?
				{
					fnDelete(*itPong++);
				}
				else // Same:
				{
					*itPing = *itPong; // Copy existing payload from pong to ping.
					itPing++;
					itPong++;
				}
			}
			// Ran out of ping elements?
			while (itPong != itPongEnd) // Pong leftovers.
				fnDelete(*itPong++);

			// Swap:
			_pong = _ping;
			_ping = (_ping + 1) & 0x1;
			_v[_ping].clear();
		}

		template<typename TFn>
		void ForEach(const TFn& fn)
		{
			for (auto& x : _v[_pong])
				fn(x);
		}
	};
}
