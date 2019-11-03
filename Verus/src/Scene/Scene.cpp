#include "verus.h"

namespace verus
{
	void Make_Scene()
	{
		Scene::Helpers::Make();
		Scene::MaterialManager::Make();
		Scene::SceneManager::Make();
		Scene::Atmosphere::Make();
	}
	void Free_Scene()
	{
		Scene::Atmosphere::Free();
		Scene::SceneManager::Free();
		Scene::MaterialManager::Free();
		Scene::Helpers::Free();
	}
}
