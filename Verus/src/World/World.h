// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

//#define VERUS_WORLD_FORCE_FLAGS

#ifdef VERUS_WORLD_FORCE_FLAGS
#	pragma message("VERUS_WORLD_FORCE_FLAGS is defined")
#endif

namespace verus
{
	namespace World
	{
		enum class DrawDepth : int
		{
			no,
			yes,
			automatic
		};

		enum class DrawSimpleMode : int
		{
			envMap,
			planarReflection
		};

		enum class DrawEditorOverlaysFlags : UINT32
		{
			none = 0,
			bounds = (1 << 0),
			extras = (1 << 1),
			unbounded = (1 << 2)
		};

		enum class HierarchyDuplication : UINT32
		{
			no = 0,
			yes = (1 << 0),
			withProbability = (1 << 1),
			generated = (1 << 2)
		};

		class ShadowMapHandle : public BaseHandle<ShadowMapHandle>
		{
		public:
			static ShadowMapHandle Make(int value) { return BaseHandle::Make(value); }

			UINT32 GetBlockIndex() const { return Get() >> 16; }
			UINT32 GetCellIndex() const { return Get() & 0xFFFF; }
		};
	}
}

#include "Camera.h"
#include "OrbitingCamera.h"

#include "BaseMesh.h"
#include "Mesh.h"

#include "Terrain.h"
#include "EditorTerrain.h"

#include "MaterialManager.h"
#include "WorldNodes/WorldNodes.h"
#include "WorldManager.h"

#include "CubeMapBaker.h"
#include "LightMapBaker.h"
#include "ShadowMapBaker.h"
#include "ShadowMapBakerPool.h"

#include "Atmosphere.h"
#include "Scatter.h"
#include "Forest.h"
#include "Grass.h"
#include "Water.h"

#include "WorldUtils.h"
#include "EditorOverlays.h"

namespace verus
{
	void Make_World();
	void Free_World();
}
