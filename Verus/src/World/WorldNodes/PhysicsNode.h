// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// PhysicsNode adds rigid bodies to the physics engine.
		// * can be static, dynamic or kinematic
		// * can have a basic shape or can take it's shape from block node or load it from file
		// * has some physical parameters like friction and restitution
		class PhysicsNode : public BaseNode
		{
		public:
			enum class ShapeType : int
			{
				parentBlock,
				fromFile,
				box,
				capsule,
				cone,
				cylinder,
				sphere
			};

			enum class BodyType : int
			{
				staticBody,
				dynamicBody,
				kinematicBody
			};

		private:
			Vector3                 _size = Vector3::Replicate(1);
			Physics::LocalRigidBody _pLocalRigidBody;
			String                  _shapeURL;
			btCollisionShape* _pCollisionShape = nullptr;
			ShapeType               _shapeType = ShapeType::parentBlock;
			BodyType                _bodyType = BodyType::staticBody;
			float                   _mass = 1;
			float                   _friction = 0.5f;
			float                   _restitution = 0.5f;
			float                   _linearDamping = 0;
			float                   _angularDamping = 0;
			UINT32                  _collisionFilterGroup = +(Physics::Group::immovable | Physics::Group::node);
			UINT32                  _collisionFilterMask = +Physics::Group::all;
			bool                    _async_loadedModel = false;

		public:
			struct Desc : BaseNode::Desc
			{
				Desc(CSZ name = nullptr) : BaseNode::Desc(name) {}
			};
			VERUS_TYPEDEFS(Desc);

			PhysicsNode();
			virtual ~PhysicsNode();

			void Init(RcDesc desc);
			void Done();

			virtual void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication) override;

			virtual void Update() override;
			virtual void DrawEditorOverlays(DrawEditorOverlaysFlags flags) override;

			// <Editor>
			virtual void GetEditorCommands(Vector<EditorCommand>& v) override;
			virtual void ExecuteEditorCommand(RcEditorCommand command) override;
			virtual bool CanAutoSelectParentNode() const override;
			// </Editor>

			virtual void Disable(bool disable) override;

			// <CollisionGroups>
			bool IsInCollisionGroup(int index) const;
			void ToggleCollisionGroup(int index);
			bool IsInCollisionMask(int index) const;
			void ToggleCollisionMask(int index);
			// </CollisionGroups>

			virtual void UpdateBounds() override;

			// <Physics>
			virtual void AddDefaultPickingBody() override {}
			virtual void RemoveRigidBody() override;
			void AddNewRigidBody(btCollisionShape* pShape, bool updateBounds = true);
			void ResetRigidBodyTransform();
			// </Physics>

			// <Events>
			virtual void OnNodeParentChanged(PBaseNode pNode, bool afterEvent) override;
			virtual void OnNodeRigidBodyTransformUpdated(PBaseNode pNode, bool afterEvent) override;
			// </Events>

			// <Serialization>
			virtual void Serialize(IO::RSeekableStream stream) override;
			virtual void Deserialize(IO::RStream stream) override;
			// </Serialization>

			// <Shape>
			ShapeType GetShapeType() const { return _shapeType; }
			void SetShapeType(ShapeType shapeType);
			void CreateCollisionShape();
			void LoadCollisionShape(CSZ url);
			Str GetCollisionShapeURL() const { return _C(_shapeURL); }
			// </Shape>

			BodyType GetBodyType() const { return _bodyType; }
			void SetBodyType(BodyType bodyType);

			RcVector3 GetSize() const { return _size; }
			void Resize(RcVector3 size);
			void TakeSizeFromParentBlock();

			float GetMass() const { return _mass; }
			void SetMass(float mass) { _mass = Math::Max(0.f, mass); }

			float GetFriction() const { return _friction; }
			void SetFriction(float friction);

			float GetRestitution() const { return _restitution; }
			void SetRestitution(float restitution);

			float GetLinearDamping() const { return _linearDamping; }
			void SetLinearDamping(float linearDamping);

			float GetAngularDamping() const { return _angularDamping; }
			void SetAngularDamping(float angularDamping);
		};
		VERUS_TYPEDEFS(PhysicsNode);

		class PhysicsNodePtr : public Ptr<PhysicsNode>
		{
		public:
			void Init(PhysicsNode::RcDesc desc);
			void Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication);
		};
		VERUS_TYPEDEFS(PhysicsNodePtr);

		class PhysicsNodePwn : public PhysicsNodePtr
		{
		public:
			~PhysicsNodePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(PhysicsNodePwn);
	}
}
