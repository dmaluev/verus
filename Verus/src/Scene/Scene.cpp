// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
	}
	void Free_Scene()
	{
		Scene::Water::Free();
		Scene::Atmosphere::Free();
		Scene::SceneManager::Free();
		Scene::MaterialManager::Free();
		Scene::Helpers::Free();
	}
}
