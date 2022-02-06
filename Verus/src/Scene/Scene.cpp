// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_Scene()
	{
		Scene::Helpers::Make();
		Scene::MaterialManager::Make();
		Scene::SceneManager::Make();
		Scene::Atmosphere::Make();
		Scene::Water::Make();
		Scene::LightMapBaker::Make();
	}
	void Free_Scene()
	{
		Scene::LightMapBaker::Free();
		Scene::Water::Free();
		Scene::Atmosphere::Free();
		Scene::SceneManager::Free();
		Scene::MaterialManager::Free();
		Scene::Helpers::Free();
	}
}
