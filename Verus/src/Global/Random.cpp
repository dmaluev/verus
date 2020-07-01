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

UINT32 Random::Next(UINT32 mn, UINT32 mx)
{
	VERUS_RT_ASSERT(mx >= mn);
	const UINT32 d = mx - mn;
	return mn + (Next() % (d + 1));
}

double Random::NextDouble(double mn, double mx)
{
	VERUS_RT_ASSERT(mx >= mn);
	const double d = mx - mn;
	return mn + d * NextDouble();
}

float Random::NextFloat(float mn, float mx)
{
	VERUS_RT_ASSERT(mx >= mn);
	const float d = mx - mn;
	return mn + d * NextFloat();
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
