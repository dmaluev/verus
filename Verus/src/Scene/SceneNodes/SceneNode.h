#pragma once

namespace verus
{
	namespace Scene
	{
		//! SceneNode is an element of the scene manager container.
		//! * has a name
		//! * can be parent or child
		//! * has generic parameters
		//! * has bounds
		//! * can be transformed using the gizmo
		class SceneNode : public AllocatorAware, public Physics::UserPtr
		{
		protected:
			Transform3     _tr = Transform3::identity(); //!< Main transformation matrix.
			Vector3        _uiRotation = Vector3(0); //!< User-friendly rotation used in editor's UI.
			Vector3        _uiScale = Vector3(1, 1, 1); //!< User-friendly scale used in editor's UI.
			Math::Bounds   _bounds;
			IO::Dictionary _dict;
			String         _name;
			String         _parent;
			btRigidBody* _pBody = nullptr;
			NodeType       _type = NodeType::unknown;
			bool           _hidden = false;
			bool           _dynamic = false;
			bool           _octreeBindOnce = false; //!< Don't rebind this node after every bounds change.
			bool           _selected = false;

			void PreventNameCollision();

		public:
			SceneNode();
			virtual ~SceneNode();

			virtual int UserPtr_GetType() override;

			virtual void Update() {}
			virtual void Draw() {}
			virtual void DrawImmediate() {}
			void DrawBounds();

			Str GetName() const { return _C(_name); }
			void Rename(CSZ name);
			NodeType GetType() const { return _type; }
			virtual String GetUrl() { return ""; }
			IO::RDictionary GetDictionary() { return _dict; }

			void Hide(bool hide = true) { _hidden = hide; }
			bool IsHidden() const { return _hidden; }

			virtual void SetDynamic(bool mode) { _dynamic = mode; }
			bool IsDynamic() const { return _dynamic; }

			void Select(bool select = true) { _selected = select; }
			bool IsSelected() const { return _selected; }

			virtual Vector4 GetColor() { return Vector4(0); }
			virtual void SetColor(RcVector4 color) {}

			RcTransform3 GetTransform() const { return _tr; }
			virtual void SetTransform(RcTransform3 tr);
			VERUS_P(void ComputeTransform());

			Point3 GetPosition() const; //!< Gets the position from the main transformation matrix.
			Vector3 GetRotation() const; //!< Gets the user-friendly rotation.
			Vector3 GetScale() const; //!< Gets the user-friendly scale.
			virtual void MoveTo(RcPoint3 pos);
			virtual void RotateTo(RcVector3 v);
			virtual void ScaleTo(RcVector3 v);

			// Bounds:
			Math::RcBounds GetBounds() const { return _bounds; }
			virtual void UpdateBounds() {}

			// Physics engine:
			void RemoveRigidBody();
			virtual void SetCollisionGroup(Physics::Group g = Physics::Group::immovable);
			virtual void MoveRigidBody();

			// Serialization:
			virtual void Serialize(IO::RSeekableStream stream);
			virtual void Deserialize(IO::RStream stream);
			virtual void SaveXML(pugi::xml_node node);
			virtual void LoadXML(pugi::xml_node node);

			float GetDistToEyeSq() const;
		};
		VERUS_TYPEDEFS(SceneNode);
	}
}
