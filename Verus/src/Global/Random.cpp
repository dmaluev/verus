#include "verus.h"

using namespace verus;

Random::Random()
{
	std::random_device rd;
	Seed(rd());
}

Random::Random(UINT32 seed) :
	_mt(seed)
{
}

Random::~Random()
{
}

UINT32 Random::Next()
{
	return _mt();
}

double Random::NextDouble()
{
	return static_cast<double>(Next()) / std::numeric_limits<UINT32>::max();
}

float Random::NextFloat()
{
	return static_cast<float>(Next()) / std::numeric_limits<UINT32>::max();
}

void Random::NextArray(UINT32* p, int count)
{
	VERUS_FOR(i, count)
		p[i] = Next();
}

void Random::NextArray(float* p, int count)
{
	VERUS_FOR(i, count)
		p[i] = NextFloat();
}

void Random::NextArray(Vector<BYTE>& v)
{
	VERUS_FOREACH(Vector<BYTE>, v, it)
		* it = Next();
}

void Random::Seed(UINT32 seed)
{
	_mt.seed(seed);
}
