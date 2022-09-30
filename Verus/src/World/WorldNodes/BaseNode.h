// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// BaseNode is a base node for all other nodes.
		// * has a name
		// * can be parent or child
		// * has generic properties
		// * has transformation
		// * has bounds
		class BaseNode : public Physics::UserPtr, public Object, public AllocatorAware
		{
		protected:
			enum class Flags : UINT32
			{
				none = 0,
				octreeElement = (1 << 0),
				octreeBindOnce = (1 << 1),
				dynamic = (1 << 2), // Will be bound to the root of octree.
				generated = (1 << 3), // Created by some other node.
				disabled = (1 << 4), // Draw method will not be called.
				selected = (1 << 5)
			};

			Transform3     _trLocal = Transform3::identity();
			Transform3     _trGlobal = Transform3::identity();
			Vector3        _uiRotation = Vector3(0); // User-friendly rotation used in editor's UI.
			Vector3        _uiScale = Vector3(1, 1, 1); // User-friendly scale used in editor's UI.
			Math::Bounds   _bounds;
			IO::Dictionary _dict;
			btRigidBody* _pRigidBody = nullptr;
			BaseNode* _pParent = nullptr;
			String         _name;
			NodeType       _type = NodeType::base;
			Flags          _flags = Flags::none;
			UINT32         _groups = 0;
			int            _depth = 0;

		public:
			struct Desc
			{
				CSZ _name = nullptr;

				Desc(CSZ name = nullptr) : _name(name) {}
			};
			VERUS_TYPEDEFS(Desc);

			BaseNode();
			virtual ~BaseNode();

			virtual int UserPtr_GetType() override;

			void Init(RcDesc desc);
			void Done();

			virtual void Duplicate(BaseNode& node);

			virtual void Update() {}
			virtual void Layout() {}
			virtual void Draw() {}
			virtual void DrawEditorOverlays(DrawEditorOverlaysFlags flags);
			void DrawBounds();
			void DrawRelationshipLines();

			// <Editor>
			virtual void GetEditorCommands(Vector<EditorCommand>& v);
			virtual void ExecuteEditorCommand(RcEditorCommand command);
			virtual bool CanAutoSelectParentNode() const { return false; }
			// </Editor>

			// <Identity>
			Str GetName() const { return _C(_name); }
			void Rename(CSZ name);
			NodeType GetType() const { return _type; }
			// </Identity>

			// <Hierarchy>
			int GetDepth() const { return _depth; }
			void UpdateDepth();

			BaseNode* GetParent() const;
			bool SetParent(BaseNode* pNode, bool keepLocalTransform = false);
			virtual bool CanSetParent(BaseNode* pNode) const { return true; }
			// </Hierarchy>

			// <Flags>
			bool IsOctreeElement(bool strict = false) const;
			void SetOctreeElementFlag(bool octreeElement = true);

			bool IsOctreeBindOnce() const;
			void SetOctreeBindOnceFlag(bool octreeBindOnce = true);

			bool IsDynamic() const;
			void SetDynamicFlag(bool dynamic = true);

			bool IsGenerated() const;
			void SetGeneratedFlag(bool generated = true);

			bool IsDisabled() const;
			virtual void Disable(bool disable = true);

			bool IsSelected() const;
			void Select(bool select = true);
			// </Flags>

			// <Groups>
			bool IsInGroup(int index) const;
			void ToggleGroup(int index);
			// </Groups>

			virtual float GetPropertyByName(CSZ name) const;
			virtual void SetPropertyByName(CSZ name, float value);

			IO::RDictionary GetDictionary() { return _dict; }
			void SetDictionary(IO::RcDictionary dict) { _dict = dict; };

			// <Transform>
			Point3 GetPosition(bool local = false) const;
			Vector3 GetRotation() const;
			Vector3 GetScale() const;
			void MoveTo(RcPoint3 pos, bool local = false);
			void RotateTo(RcVector3 v);
			void ScaleTo(RcVector3 v);
			void SetDirection(RcVector3 dir);

			RcTransform3 GetTransform(bool local = false) const;
			void SetTransform(RcTransform3 tr, bool local = false);
			void OverrideGlobalTransform(RcTransform3 tr);
			void RestoreTransform(RcTransform3 trLocal, RcVector3 rot, RcVector3 scale);
			void UpdateLocalTransform(bool updateUiValues = true);
			void UpdateGlobalTransform(bool updateBounds = true);
			void UiToLocalTransform();
			void UiFromLocalTransform();
			virtual void OnLocalTransformUpdated() {} // Can be used to adjust local transform.
			// </Transform>

			Math::RcBounds GetBounds() const { return _bounds; }
			virtual void UpdateBounds();

			// <Physics>
			virtual void AddDefaultPickingBody();
			virtual void RemoveRigidBody();
			void SetCollisionFilter(UINT32 group, UINT32 mask);
			void UpdateRigidBodyTransform();
			// </Physics>

			// <Events>
			virtual void OnNodeDeleted(BaseNode* pNode, bool afterEvent, bool hierarchy);
			virtual void OnNodeDuplicated(BaseNode* pNode, bool afterEvent, BaseNode* pDuplicatedNode, HierarchyDuplication hierarchyDuplication);
			virtual void OnNodeParentChanged(BaseNode* pNode, bool afterEvent);
			virtual void OnNodeRigidBodyTransformUpdated(BaseNode* pNode, bool afterEvent);
			virtual void OnNodeTransformed(BaseNode* pNode, bool afterEvent);
			// </Events>

			// <Serialization>
			virtual void Serialize(IO::RSeekableStream stream);
			virtual void Deserialize(IO::RStream stream);
			void Deserialize_LegacyXXX(IO::RStream stream);
			void Deserialize_LegacyXXXPrefab(IO::RStream stream);
			virtual void SerializeXML(pugi::xml_node node);
			virtual void DeserializeXML(pugi::xml_node node);
			void DeserializeXML_LegacyXXX(pugi::xml_node node);
			virtual void OnAllNodesDeserialized() {}
			// </Serialization>

			float GetDistToHeadSq() const;
		};
		VERUS_TYPEDEFS(BaseNode);

		class BaseNodePtr : public Ptr<BaseNode>
		{
		public:
			void Init(BaseNode::RcDesc desc);
			void Duplicate(RBaseNode node);
		};
		VERUS_TYPEDEFS(BaseNodePtr);

		class BaseNodePwn : public BaseNodePtr
		{
		public:
			~BaseNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(BaseNodePwn);
	}
}
