// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

int Range::GetCount() const
{
	return _end - _begin;
}

int Range::GetRandomValue() const
{
	const int count = GetCount();
	if (count > 0)
		return Utils::I().GetRandom().Next(_begin, _end - 1);
	return 0;
}

Range& Range::OffsetBy(int x)
{
	_begin += x;
	_end += x;
	return *this;
}

Range::Iterator Range::begin() const
{
	return _begin;
}

Range::Iterator Range::end() const
{
	return _end;
}
