// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	// PrefabNode can create and update InstanceNodes, which can duplicate child nodes of PrefabNode.
	// * child nodes can have "PrefabDupP" property, which controls whether they will get duplicated
	// * has seed, which affects the "PrefabDupP" property
	class PrefabNode : public BaseNode
	{
		UINT16 _seed = 0;

	public:
		struct Desc : BaseNode::Desc
		{
			Desc(CSZ name = nullptr) : BaseNode::Desc(name) {}
		};
		VERUS_TYPEDEFS(Desc);

		PrefabNode();
		virtual ~PrefabNode();

		void Init(RcDesc desc);
		void Done();

		virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

		virtual void GetEditorCommands(Vector<EditorCommand>& v) override;

		virtual bool CanSetParent(PBaseNode pNode) const override;

		virtual void Serialize(IO::RSeekableStream stream) override;
		virtual void Deserialize(IO::RStream stream) override;

		void DeleteAllInstances();

		UINT16 GetSeed() const { return _seed; }
		void SetSeed(UINT16 seed) { _seed = seed; }
	};
	VERUS_TYPEDEFS(PrefabNode);

	class PrefabNodePtr : public Ptr<PrefabNode>
	{
	public:
		void Init(PrefabNode::RcDesc desc);
		void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
	};
	VERUS_TYPEDEFS(PrefabNodePtr);

	class PrefabNodePwn : public PrefabNodePtr
	{
	public:
		~PrefabNodePwn() { Done(); }
		void Done();
	};
	VERUS_TYPEDEFS(PrefabNodePwn);
}
