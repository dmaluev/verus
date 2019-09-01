#include "verus.h"

using namespace verus;
using namespace verus::Scene;

SceneManager::SceneManager()
{
}

SceneManager::~SceneManager()
{
	Done();
}

void SceneManager::Init()
{
	VERUS_INIT();
}

void SceneManager::Done()
{
	VERUS_DONE(SceneManager);
}
