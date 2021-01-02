// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
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
	}
}

#include "Camera.h"
#include "CameraOrbit.h"
#include "BaseMesh.h"
#include "Mesh.h"
#include "Terrain.h"
#include "EditorTerrain.h"
#include "Grass.h"
#include "CubeMap.h"
#include "ShadowMap.h"
#include "Water.h"
#include "MaterialManager.h"
#include "SceneNodes/SceneNodes.h"
#include "SceneManager.h"
#include "Atmosphere.h"
#include "Helpers.h"
#include "Scatter.h"
#include "Forest.h"

namespace verus
{
	void Make_Scene();
	void Free_Scene();
}
