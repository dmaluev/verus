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

bool SceneManager::IsDrawingDepth(DrawDepth dd)
{
	if (DrawDepth::automatic == dd)
	{
		VERUS_QREF_ATMO;
		return atmo.IsRenderingShadow();
	}
	else
		return DrawDepth::yes == dd;
}
