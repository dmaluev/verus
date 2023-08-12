// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	enum class NodeType : int
	{
		unknown,

		base,

		model,
		particles,

		ambient,
		block,
		blockChain,
		controlPoint,
		emitter,
		instance,
		light,
		path,
		physics,
		prefab,
		project,
		shaker,
		sound,
		terrain,

		trigger,

		site,

		character,
		vehicle,

		count
	};
}
