// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		class BlockChainNode : public BaseNode
		{
		public:
			struct SourceModel
			{
				Vector<Range> _vSlots;
				ModelNodePtr  _modelNode;
				String        _arguments;
				float         _length = 0;
				bool          _noPhysicsNode = false;
				bool          _noShadow = false;

				bool IsAssignedToSlot(int slot) const;
			};
			VERUS_TYPEDEFS(SourceModel);

		private:
			Vector<SourceModel> _vSourceModels;
			bool                _async_generatedNodes = false;

		public:
			struct Desc : BaseNode::Desc
			{
				PBaseNode _pPathNode = nullptr;

				Desc(PBaseNode pPathNode = nullptr) : _pPathNode(pPathNode) {}
			};
			VERUS_TYPEDEFS(Desc);

			BlockChainNode();
			virtual ~BlockChainNode();

			void Init(RcDesc desc);
			void Done();

			virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

			virtual void Update() override;

			virtual void GetEditorCommands(Vector<EditorCommand>& v) override;
			virtual void ExecuteEditorCommand(RcEditorCommand command) override;

			virtual bool CanSetParent(PBaseNode pNode) const override;

			virtual void OnNodeDeleted(PBaseNode pNode, bool afterEvent, bool hierarchy) override;

			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;

			bool BindSourceModel(PModelNode pModelNode);
			bool UnbindSourceModel(PModelNode pModelNode);
			int GetSourceModelCount() const;
			PcSourceModel GetSourceModelForSlot(int slot) const;
			ModelNodePtr GetModelNodeForSlot(int slot) const;

			template<typename T>
			void ForEachSourceModel(const T& fn)
			{
				for (auto& x : _vSourceModels)
					fn(x);
			}

			float GetLengthForSlot(int slot) const;

			void ParseArguments();
		};
		VERUS_TYPEDEFS(BlockChainNode);

		class BlockChainNodePtr : public Ptr<BlockChainNode>
		{
		public:
			void Init(BlockChainNode::RcDesc desc);
			void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
		};
		VERUS_TYPEDEFS(BlockChainNodePtr);

		class BlockChainNodePwn : public BlockChainNodePtr
		{
		public:
			~BlockChainNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(BlockChainNodePwn);
	}
}
