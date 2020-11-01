// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Physics
	{
		enum class Group : short
		{
			none = 0,
			general = (1 << 0),
			immovable = (1 << 1),
			kinematic = (1 << 2),
			debris = (1 << 3),
			sensor = (1 << 4),
			character = (1 << 5),
			terrain = (1 << 6),
			sceneBounds = (1 << 7),
			gizmo = (1 << 8),
			pickable = (1 << 9),
			particle = (1 << 10),
			vehicle = (1 << 11),
			all = -1
		};
	}
}
