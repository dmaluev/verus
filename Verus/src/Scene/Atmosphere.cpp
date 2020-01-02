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
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;

	if (settings._sceneShadowQuality > App::Settings::ShadowQuality::none)
		_shadowMap.Init(4096);

	renderer.GetDS().InitByAtmosphere(_shadowMap.GetTexture());

	_sun._dirTo = VMath::normalize(Vector3(1.3f, 1, 1.3f));
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

void Atmosphere::BeginShadow(int split)
{
	_shadowMap.Begin(_sun._dirTo, 1, 0, split);
}

void Atmosphere::EndShadow(int split)
{
	_shadowMap.End(split);
}

RcPoint3 Atmosphere::GetEyePosition(PVector3 pDirFront)
{
	VERUS_QREF_SM;
	if (_shadowMap.GetSceneCamera())
	{
		if (pDirFront)
			*pDirFront = _shadowMap.GetSceneCamera()->GetFrontDirection();
		return _shadowMap.GetSceneCamera()->GetEyePosition();
	}
	else
	{
		if (pDirFront)
			*pDirFront = sm.GetCamera()->GetFrontDirection();
		return sm.GetCamera()->GetEyePosition();
	}
}
