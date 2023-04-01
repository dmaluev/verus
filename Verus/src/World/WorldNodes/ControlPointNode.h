// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// ControlPointNode is a child of PathNode, that defines the shape of the path.
		// * can be connected to other control points
		// * can form smooth or straight segment
		class ControlPointNode : public BaseNode
		{
			ControlPointNode* _pPrev = nullptr;
			ControlPointNode* _pNext = nullptr;
			float             _segmentLength = 0;
			bool              _smoothSegment = true;
			bool              _markedAsHead = false;

		public:
			struct Desc : BaseNode::Desc
			{
				PBaseNode _pPathNode = nullptr;

				Desc(PBaseNode pPathNode = nullptr) : _pPathNode(pPathNode) {}
			};
			VERUS_TYPEDEFS(Desc);

			ControlPointNode();
			virtual ~ControlPointNode();

			void Init(RcDesc desc);
			void Done();

			virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

			virtual void DrawEditorOverlays(DrawEditorOverlaysFlags flags) override;

			virtual void GetEditorCommands(Vector<EditorCommand>& v) override;
			virtual bool CanAutoSelectParentNode() const override;

			virtual bool CanSetParent(PBaseNode pNode) const override;

			virtual void OnNodeDeleted(PBaseNode pNode, bool afterEvent, bool hierarchy) override;
			virtual void OnNodeParentChanged(PBaseNode pNode, bool afterEvent) override;
			virtual void OnNodeTransformed(PBaseNode pNode, bool afterEvent) override;

			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;
			virtual void OnAllNodesDeserialized() override;

			PBaseNode GetPathNode() const;

			ControlPointNode* GetPreviousControlPoint() const { return _pPrev; }
			ControlPointNode* GetNextControlPoint() const { return _pNext; }
			void SetPreviousControlPoint(ControlPointNode* p) { _pPrev = p; }
			void SetNextControlPoint(ControlPointNode* p) { _pNext = p; UpdateSegmentLength(); }

			void InsertControlPoint(ControlPointNode* pTargetNode, PcPoint3 pPos = nullptr);

			bool DisconnectPreviousControlPoint();
			bool DisconnectNextControlPoint();

			bool IsHeadControlPoint() const;
			bool IsTailControlPoint() const;
			bool IsIsolatedControlPoint() const;

			float GetSegmentLength() const { return _segmentLength; }
			void UpdateSegmentLength();
			void GetSegmentData(glm::vec3 pos4[4], float len3[3]) const;
			void GetSegmentTangents(glm::vec3 pos4[4], float len3[3], glm::vec3 tan2[2]) const;

			bool IsSmoothSegment() const { return _smoothSegment; }
			void SetSmoothSegment(bool smooth) { _smoothSegment = smooth; }

			bool IsMarkedAsHead() const { return _markedAsHead; }
			void MarkAsHead(bool head) { _markedAsHead = head; }

			bool ComputePositionAt(float distance, RPoint3 pos, PVector3 pDir = nullptr, bool extrapolate = false) const;
		};
		VERUS_TYPEDEFS(ControlPointNode);

		class ControlPointNodePtr : public Ptr<ControlPointNode>
		{
		public:
			void Init(ControlPointNode::RcDesc desc);
			void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
		};
		VERUS_TYPEDEFS(ControlPointNodePtr);

		class ControlPointNodePwn : public ControlPointNodePtr
		{
		public:
			~ControlPointNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(ControlPointNodePwn);
	}
}
