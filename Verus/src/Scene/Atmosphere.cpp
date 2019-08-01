#include "verus.h"

using namespace verus;
using namespace verus::Scene;

Atmosphere::Atmosphere()
{
}

Atmosphere::~Atmosphere()
{
	Done();
}

void Atmosphere::Init()
{
	VERUS_INIT();
}

void Atmosphere::Done()
{
	VERUS_DONE(Atmosphere);
}

RcVector3 Atmosphere::GetDirToSun() const
{
	return _sun._dirTo;
}

RcVector3 Atmosphere::GetSunColor() const
{
	return _sun._color;
}
