// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

float Interval::GetLength() const
{
	return _max - _min;
}

float Interval::GetRandomValue() const
{
	return _min + Utils::I().GetRandom().NextFloat() * GetLength();
}
