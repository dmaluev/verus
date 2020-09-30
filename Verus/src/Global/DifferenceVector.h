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
		void PushBack(const T& x)
		{
			_v[_ping].push_back(x);
		}

		template<typename TFnInsert, typename TFnDelete>
		void HandleDifference(const TFnInsert& fnInsert, const TFnDelete& fnDelete)
		{
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
				if (itPong == itPongEnd) // No more items in pong?
				{
					while (itPing != itPingEnd) // All remaining.
						fnInsert(*itPing++);
					break;
				}

				if (itPing->GetID() < itPong->GetID()) // New item in ping, insert it:
				{
					fnInsert(*itPing++);
				}
				else if (itPong->GetID() < itPing->GetID()) // Old item in pong, delete it.
				{
					fnDelete(*itPong++);
				}
				else // Same:
				{
					*itPing = *itPong; // No need to insert or delete, just copy payload.
					itPing++;
					itPong++;
				}
			}
			while (itPong != itPongEnd) // Handle leftovers.
				fnDelete(*itPong++);

			_pong = _ping;
			_ping = (_ping + 1) & 0x1;
			_v[_ping].clear();
		}
	};
}
