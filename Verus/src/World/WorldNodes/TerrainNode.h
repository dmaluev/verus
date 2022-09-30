// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// TerrainNode adds terrain.
		class TerrainNode : public BaseNode
		{
			EditorTerrain _terrain;

		public:
			struct Desc : BaseNode::Desc
			{
				Terrain::Desc _terrainDesc;
			};
			VERUS_TYPEDEFS(Desc);

			TerrainNode();
			virtual ~TerrainNode();

			void Init(RcDesc desc);
			void Done();

			virtual void Layout() override;
			virtual void Draw() override;

			virtual void AddDefaultPickingBody() override {}

			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;

			// <Resources>
			RTerrain GetTerrain() { return _terrain; }
			REditorTerrain GetEditorTerrain() { return _terrain; }
			// </Resources>
		};
		VERUS_TYPEDEFS(TerrainNode);

		class TerrainNodePtr : public Ptr<TerrainNode>
		{
		public:
			void Init(TerrainNode::RcDesc desc);
		};
		VERUS_TYPEDEFS(TerrainNodePtr);

		class TerrainNodePwn : public TerrainNodePtr
		{
		public:
			~TerrainNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(TerrainNodePwn);
	}
}
