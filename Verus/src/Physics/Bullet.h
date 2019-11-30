#pragma once

namespace verus
{
	namespace Physics
	{
		enum class Material : int
		{
			brick,
			glass,
			leather,
			metal,
			plastic,
			rubber,
			sand,
			stone,
			wood
		};

		class Bullet : public Singleton<Bullet>, public Object
		{
			static const int s_defaultMaxSubSteps = 8;

			btDispatcher* _pDispatcher = nullptr;
			btBroadphaseInterface* _pBroadphaseInterface = nullptr;
			btConstraintSolver* _pConstraintSolver = nullptr;
			btCollisionConfiguration* _pCollisionConfiguration = nullptr;
			btDiscreteDynamicsWorld* _pDiscreteDynamicsWorld = nullptr;
			btGhostPairCallback _ghostPairCallback;
			BulletDebugDraw _debugDraw;

		public:
			Bullet();
			~Bullet();

			void Init();
			void Done();

			btDiscreteDynamicsWorld* GetWorld() { return _pDiscreteDynamicsWorld; }

			btRigidBody* AddNewRigidBody(
				float mass,
				const btTransform& startTransform,
				btCollisionShape* pShape,
				short group = 1,
				short mask = -1,
				const btTransform* pCenterOfMassOffset = nullptr);

			void DeleteAllCollisionObjects();

			void Simulate();

			void DebugDraw();
			void EnableDebugDraw(bool b);

			static float GetFriction(Material m);
			static float GetRestitution(Material m);
		};
		VERUS_TYPEDEFS(Bullet);
	}
}
