// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
		class Xxx : public AsyncDelegate
		{
			Scene::Terrain* _pTerrain = nullptr;

		public:
			static void Serialize(CSZ path);
			static void Deserialize(CSZ path, RString error, bool keepExisting = false);

			Xxx(Scene::Terrain* pTerrain = nullptr);
			~Xxx();

			virtual void Async_WhenLoaded(CSZ url, RcBlob blob) override;

			void SerializeXXX3(CSZ url);
			void DeserializeXXX3(CSZ url, bool sync);

			static UINT32 MakeVersion(UINT32 verMajor, UINT32 verMinor);
		};
	}
}
