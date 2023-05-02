// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// TerrainNode:

TerrainNode::TerrainNode()
{
	_type = NodeType::terrain;
}

TerrainNode::~TerrainNode()
{
	Done();
}

void TerrainNode::Init(RcDesc desc)
{
	if (!(_flags & Flags::readOnlyFlags))
	{
		SetShadowFlag();
	}
	BaseNode::Init(desc._name ? desc._name : "Terrain");
#ifdef VERUS_WORLD_FORCE_FLAGS
	SetShadowFlag();
#endif

	if (desc._terrainDesc._mapSide)
		_terrain.Init(desc._terrainDesc);
}

void TerrainNode::Done()
{
	_terrain.Done();

	VERUS_DONE(TerrainNode);
}

void TerrainNode::Layout()
{
	_terrain.Layout();
}

void TerrainNode::Draw()
{
	_terrain.Draw();
}

void TerrainNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	_terrain.Serialize(stream);
}

void TerrainNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	_terrain.Deserialize(stream);

	if (NodeType::terrain == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._terrainDesc._mapSide = 0;
		Init(desc);
	}
}

// TerrainNodePtr:

void TerrainNodePtr::Init(TerrainNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertTerrainNode();
	_p->Init(desc);
}

void TerrainNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
