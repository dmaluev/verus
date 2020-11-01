// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		//! SceneManager will sort nodes using their type.
		enum class NodeType : int
		{
			unknown,
			block,
			light,
			prefab,
			emitter,
			site,
			terrain,
			character,
			vehicle,
			transformGizmo,
			count
		};
	}
}
