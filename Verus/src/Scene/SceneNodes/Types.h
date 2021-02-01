// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		enum class NodeType : int
		{
			unknown,

			block,
			emitter,
			light,
			prefab,
			trigger,

			site,

			terrain,
			character,
			vehicle,

			count
		};
	}
}
