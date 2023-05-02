// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_World()
	{
		World::WorldUtils::Make();
		World::EditorOverlays::Make();
		World::CascadedShadowMapBaker::Make();
		World::ShadowMapBakerPool::Make();
		World::MaterialManager::Make();
		World::WorldManager::Make();
		World::Atmosphere::Make();
		World::Water::Make();
		World::LightMapBaker::Make();
	}
	void Free_World()
	{
		World::LightMapBaker::Free();
		World::Water::Free();
		World::Atmosphere::Free();
		World::WorldManager::Free();
		World::MaterialManager::Free();
		World::ShadowMapBakerPool::Free();
		World::CascadedShadowMapBaker::Free();
		World::EditorOverlays::Free();
		World::WorldUtils::Free();
	}
}
