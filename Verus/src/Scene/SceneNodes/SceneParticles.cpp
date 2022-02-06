// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// SceneParticles:

SceneParticles::SceneParticles()
{
}

SceneParticles::~SceneParticles()
{
	Done();
}

void SceneParticles::Init(RcDesc desc)
{
	if (_refCount)
		return;

	VERUS_INIT();
	_refCount = 1;

	_particles.Init(desc._url);
}

bool SceneParticles::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
		VERUS_DONE(SceneParticles);
		return true;
	}
	return false;
}

void SceneParticles::Update()
{
	_particles.Update();
}

void SceneParticles::Draw()
{
	_particles.Draw();
}

// SceneParticlesPtr:

void SceneParticlesPtr::Init(SceneParticles::RcDesc desc)
{
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(!_p);
	_p = sm.InsertSceneParticles(desc._url);
	_p->Init(desc);
}

void SceneParticlesPwn::Done()
{
	if (_p)
	{
		SceneManager::I().DeleteSceneParticles(_C(_p->GetParticles().GetUrl()));
		_p = nullptr;
	}
}
