// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// InstanceNode can duplicate child nodes of PrefabNode.
		// * references prefab
		class InstanceNode : public BaseNode
		{
			PBaseNode _pPrefabNode = nullptr;

		public:
			struct Desc : BaseNode::Desc
			{
				PBaseNode _pPrefabNode = nullptr;

				Desc(PBaseNode pPrefabNode = nullptr) : _pPrefabNode(pPrefabNode) {}
			};
			VERUS_TYPEDEFS(Desc);

			InstanceNode();
			virtual ~InstanceNode();

			void Init(RcDesc desc);
			void Done();

			virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;

			PBaseNode GetPrefabNode() const { return _pPrefabNode; }
		};
		VERUS_TYPEDEFS(InstanceNode);

		class InstanceNodePtr : public Ptr<InstanceNode>
		{
		public:
			void Init(InstanceNode::RcDesc desc);
			void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
		};
		VERUS_TYPEDEFS(InstanceNodePtr);

		class InstanceNodePwn : public InstanceNodePtr
		{
		public:
			~InstanceNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(InstanceNodePwn);
	}
}
