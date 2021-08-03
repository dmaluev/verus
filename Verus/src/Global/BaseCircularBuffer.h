// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	// Uses a single, fixed-size buffer as if it was connected end-to-end.
	class BaseCircularBuffer
	{
		BYTE* _pBase = nullptr;
		BYTE* _pR = nullptr;
		BYTE* _pW = nullptr;
		int   _maxItems = 0;
		int   _itemCount = 0;
		int   _maxItemSize = 0;

	public:
		BaseCircularBuffer(BYTE* pBase = nullptr, int maxItems = 1, int maxItemSize = 1) :
			_pBase(pBase),
			_maxItems(maxItems),
			_itemCount(0),
			_maxItemSize(maxItemSize)
		{
			_pR = _pW = _pBase;
		}

		bool Push(const void* p, int size = 0)
		{
			VERUS_RT_ASSERT(size <= _maxItemSize);
			if (IsFull())
				return false;
			memcpy(_pW, p, size ? size : _maxItemSize);
			_itemCount++;
			_pW += _maxItemSize;
			const int maxBytes = _maxItems * _maxItemSize;
			if (_pW >= _pBase + maxBytes)
				_pW -= maxBytes;
			return true;
		}

		bool Pop(void* p, bool justCopy = false)
		{
			if (IsEmpty())
				return false;
			if (p)
				memcpy(p, _pR, _maxItemSize);
			if (justCopy)
				return true;
			_itemCount--;
			_pR += _maxItemSize;
			const int maxBytes = _maxItems * _maxItemSize;
			if (_pR >= _pBase + maxBytes)
				_pR -= maxBytes;
			return true;
		}

		void Clear()
		{
			_pR = _pW = _pBase;
			_itemCount = 0;
		}

		BYTE* FindItem(const void* pStart, int size, int offset = 0)
		{
			BYTE* pR = _pR;
			const int maxBytes = _maxItems * _maxItemSize;
			VERUS_FOR(i, _itemCount)
			{
				if (!memcmp(pR + offset, pStart, size))
					return pR;
				pR += _maxItemSize;
				if (pR >= _pBase + maxBytes)
					pR -= maxBytes;
			}
			return nullptr;
		}

		bool MoveToFront(BYTE* p)
		{
			if (p != _pR)
			{
				VERUS_FOR(i, _maxItemSize)
					std::swap(p[i], _pR[i]);
				return true;
			}
			return false;
		}

		bool IsEmpty()       const { return !_itemCount; }
		bool IsFull()        const { return _itemCount == _maxItems; }
		int GetMaxItems()    const { return _maxItems; }
		int GetItemCount()   const { return _itemCount; }
		int GetMaxItemSize() const { return _maxItemSize; }
	};
	VERUS_TYPEDEFS(BaseCircularBuffer);
}
