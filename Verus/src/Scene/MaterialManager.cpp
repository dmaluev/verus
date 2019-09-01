#include "verus.h"

using namespace verus;
using namespace verus::Scene;

MaterialManager::MaterialManager()
{
}

MaterialManager::~MaterialManager()
{
	Done();
}

void MaterialManager::Init()
{
	VERUS_INIT();
}

void MaterialManager::Done()
{
	VERUS_DONE(MaterialManager);
}
