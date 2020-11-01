// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#include "Camera.h"
#include "CameraOrbit.h"
#include "BaseMesh.h"
#include "Mesh.h"
#include "Terrain.h"
#include "EditorTerrain.h"
#include "Grass.h"
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
