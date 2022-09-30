// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		enum class EditorCommandCode : int
		{
			custom,
			separator,
			node_delete,
			node_deleteHierarchy,
			node_duplicate,
			node_duplicateHierarchy,
			node_goTo,
			node_updateLocalTransform,
			node_resetLocalTransform,
			node_resetLocalTransformKeepGlobal,
			node_pointAt,
			model_insertBlockNode,
			model_insertBlockAndPhysicsNodes,
			model_selectAllBlocks,
			particles_insertEmitterNode,
			particles_insertEmitterAndSoundNodes,
			particles_selectAllEmitters,
			controlPoint_insertControlPoint,
			path_insertControlPoint,
			physics_takeSizeFromParentBlock,
			physics_loadShapeFromFile,
			physics_quickDynamicBox,
			physics_quickStaticParentBlock,
			prefab_insertInstanceNode,
			prefab_updateInstances,
			prefab_replaceSimilarWithInstances,
			shaker_presetForIntensity
		};

		struct EditorCommand
		{
			CSZ               _text = nullptr;
			EditorCommandCode _code = EditorCommandCode::custom;

			EditorCommand(CSZ text = nullptr, EditorCommandCode code = EditorCommandCode::custom) : _text(text), _code(code) {}
		};
		VERUS_TYPEDEFS(EditorCommand);
	}
}

#include "Types.h"
#include "BaseNode.h"

#include "ModelNode.h"
#include "ParticlesNode.h"

#include "BlockNode.h"
#include "ControlPointNode.h"
#include "EmitterNode.h"
#include "InstanceNode.h"
#include "LightNode.h"
#include "PathNode.h"
#include "PhysicsNode.h"
#include "PrefabNode.h"
#include "ShakerNode.h"
#include "SoundNode.h"
#include "TerrainNode.h"
//#include "Trigger.h"

//#include "Site.h"
