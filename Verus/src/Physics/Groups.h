// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Physics
	{
		enum class Group : UINT32
		{
			none = 0,
			general = (1 << 0),
			immovable = (1 << 1),
			kinematic = (1 << 2),
			debris = (1 << 3),
			sensor = (1 << 4),
			character = (1 << 5),
			ragdoll = (1 << 6),
			ray = (1 << 7), // rayTest
			node = (1 << 8), // World::PBaseNode
			dynamic = (1 << 9),
			gizmo = (1 << 10), // Editor::PGizmoTool
			particle = (1 << 11),
			terrain = (1 << 12),
			forest = (1 << 13),
			transport = (1 << 14),
			wall = (1 << 15),
			all = UINT32_MAX
		};
	}
}
