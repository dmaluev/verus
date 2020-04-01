#pragma once

namespace verus
{
	namespace Scene
	{
		class Terrain;
	}
}

namespace verus
{
	namespace IO
	{
		class Xxx : public AsyncCallback
		{
			Scene::Terrain* _pTerrain;

		public:
			static void Serialize(CSZ path);
			static void Deserialize(CSZ path, RString error, bool keepExisting = false);

			Xxx(Scene::Terrain* pTerrain = nullptr);
			~Xxx();

			virtual void Async_Run(CSZ url, RcBlob blob) override;

			void SerializeXXX3(CSZ url);
			void DeserializeXXX3(CSZ url, bool sync);
		};
	}
}
