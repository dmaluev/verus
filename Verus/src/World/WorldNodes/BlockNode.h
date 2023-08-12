// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	// BlockNode is like an instance of a ModelNode.
	// Most blocks form the world's so called static or immovable geometry.
	// * references model
	// * references material
	// * can have it's own material or just reuse model's material
	class BlockNode : public BaseNode
	{
		Vector4      _userColor = Vector4(0);
		ModelNodePwn _modelNode;
		MaterialPwn  _material;
		int          _materialIndex = 0;
		bool         _async_loadedModel = false;

	public:
		struct Desc : BaseNode::Desc
		{
			CSZ _modelURL = nullptr;
			CSZ _materialURL = nullptr;
		};
		VERUS_TYPEDEFS(Desc);

		BlockNode();
		virtual ~BlockNode();

		void Init(RcDesc desc);
		void Done();

		virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

		virtual void Update() override;

		virtual void UpdateBounds() override;

		virtual void Serialize(IO::RSeekableStream stream) override;
		virtual void Deserialize(IO::RStream stream) override;
		void Deserialize_LegacyXXX(IO::RStream stream);
		void LoadExtra_LegacyXXX(SZ xml);

		// <Resources>
		bool IsModelLoaded() const { return _modelNode->IsLoaded(); }
		bool IsMaterialLoaded() const { return _material->IsLoaded(); }
		void LoadMaterial(CSZ url);
		void RemoveMaterial();
		Str GetURL() const { return _modelNode->GetURL(); }

		ModelNodePtr GetModelNode() { return _modelNode; }
		MaterialPtr GetMaterial(bool orModelMat = true);
		int GetMaterialIndex() const { return _materialIndex; }
		// </Resources>

		RcVector4 GetColor() { return _userColor; }
		void SetColor(RcVector4 color) { _userColor = color; }
	};
	VERUS_TYPEDEFS(BlockNode);

	class BlockNodePtr : public Ptr<BlockNode>
	{
	public:
		void Init(BlockNode::RcDesc desc);
		void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
	};
	VERUS_TYPEDEFS(BlockNodePtr);

	class BlockNodePwn : public BlockNodePtr
	{
	public:
		~BlockNodePwn() { Done(); }
		void Done();
	};
	VERUS_TYPEDEFS(BlockNodePwn);
}
