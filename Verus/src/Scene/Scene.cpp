#include "verus.h"

namespace verus
{
	void Make_Scene()
	{
		Scene::Helpers::Make();
		Scene::MaterialManager::Make();
		Scene::SceneManager::Make();
	}
	void Free_Scene()
	{
		Scene::SceneManager::Free();
		Scene::MaterialManager::Free();
		Scene::Helpers::Free();
	}
}
