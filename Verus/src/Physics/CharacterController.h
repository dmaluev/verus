#pragma once

namespace verus
{
	namespace Physics
	{
		class CharacterController : public Object, public UserPtr
		{
			LocalPtr<btCapsuleShape>               _pCapsule;
			LocalPtr<KinematicCharacterController> _pKCC; // Action.
			LocalPtr<btPairCachingGhostObject>     _pGhostObject; // CollisionObject.
			float                                  _radius = 1;
			float                                  _height = 1;
			float                                  _offset = 0;

		public:
			class Desc
			{
			public:
				float _radius = -1;
				float _height = -1;
				float _stepHeight = -1;
			};
			VERUS_TYPEDEFS(Desc);

			CharacterController();
			~CharacterController();

			void Init(RcPoint3 pos, RcDesc desc);
			void Done();

			virtual int UserPtr_GetType() override;

			float GetRadius() const { return _radius; }
			float GetHeight() const { return _height; }
			float GetOffset() const { return _offset; }
			void SetRadius(float r);
			void SetHeight(float h);
			VERUS_P(void UpdateScaling());

			KinematicCharacterController* GetKCC() { return _pKCC.Get(); }
			const KinematicCharacterController* GetKCC() const { return _pKCC.Get(); }

			void Move(RcVector3 velocity);

			Point3 GetPosition();
			void MoveTo(RcPoint3 pos);

			void Visualize(bool b);

			bool Is(const void* p) const { return p == this; }
		};
		VERUS_TYPEDEFS(CharacterController);
	}
}
