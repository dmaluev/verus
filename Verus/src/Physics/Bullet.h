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

		enum class DebugDrawMode : int
		{
			none,
			basic
		};

		class Bullet : public Singleton<Bullet>, public Object
		{
			static const int s_defaultMaxSubSteps = 8;

			btDispatcher* _pDispatcher = nullptr;
			btBroadphaseInterface* _pBroadphaseInterface = nullptr;
			btConstraintSolver* _pConstraintSolver = nullptr;
			btCollisionConfiguration* _pCollisionConfiguration = nullptr;
			btDiscreteDynamicsWorld* _pDiscreteDynamicsWorld = nullptr;
			btStaticPlaneShape* _pStaticPlaneShape = nullptr;
			btRigidBody* _pStaticPlaneRigidBody = nullptr;
			btGhostPairCallback _ghostPairCallback;
			BulletDebugDraw _debugDraw;
			bool _pauseSimulation = false;

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
			void PauseSimualtion(bool b) { _pauseSimulation = b; }
			bool IsSimulationPaused() const { return _pauseSimulation; }

			void DebugDraw();
			void SetDebugDrawMode(DebugDrawMode mode);
			void EnableDebugPlane(bool b);

			static float GetFriction(Material m);
			static float GetRestitution(Material m);
		};
		VERUS_TYPEDEFS(Bullet);
	}
}
