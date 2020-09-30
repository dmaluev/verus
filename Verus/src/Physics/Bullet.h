#pragma once

namespace verus
{
	namespace Physics
	{
		class alignas(btDefaultMotionState) alignas(btRigidBody) LocalRigidBody
		{
			BYTE _data[sizeof(btDefaultMotionState) + sizeof(btRigidBody)] = {};
			btRigidBody* _p = nullptr;

		public:
			void operator=(btRigidBody* p)
			{
				_p = p;
			}

			btRigidBody* operator->() const
			{
				return _p;
			}
			btRigidBody* Get()
			{
				return _p;
			}

			BYTE* GetDefaultMotionStateData()
			{
				return &_data[0];
			}
			BYTE* GetRigidBodyData()
			{
				return &_data[sizeof(btDefaultMotionState)];
			}

			void Delete()
			{
				if (_p)
				{
					_p->getMotionState()->~btMotionState();
					_p->~btRigidBody();
					_p = nullptr;
				}
			}
		};
		VERUS_TYPEDEFS(LocalRigidBody);

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

			LocalPtr<btDefaultCollisionConfiguration>     _pCollisionConfiguration;
			LocalPtr<btCollisionDispatcher>               _pDispatcher;
			LocalPtr<btDbvtBroadphase>                    _pBroadphaseInterface;
			LocalPtr<btSequentialImpulseConstraintSolver> _pConstraintSolver;
			LocalPtr<btDiscreteDynamicsWorld>             _pDiscreteDynamicsWorld;
			LocalPtr<btStaticPlaneShape>                  _pStaticPlaneShape;
			LocalRigidBody                                _pStaticPlaneRigidBody;
			btGhostPairCallback                           _ghostPairCallback;
			BulletDebugDraw                               _debugDraw;
			bool                                          _pauseSimulation = false;

		public:
			Bullet();
			~Bullet();

			void Init();
			void Done();

			btDiscreteDynamicsWorld* GetWorld() { return _pDiscreteDynamicsWorld.Get(); }

			btRigidBody* AddNewRigidBody(
				float mass,
				const btTransform& startTransform,
				btCollisionShape* pShape,
				Group group = Group::general,
				Group mask = Group::all,
				const btTransform* pCenterOfMassOffset = nullptr,
				void* pPlacementMotionState = nullptr,
				void* pPlacementRigidBody = nullptr);
			btRigidBody* AddNewRigidBody(
				RLocalRigidBody localRigidBody,
				float mass,
				const btTransform& startTransform,
				btCollisionShape* pShape,
				Group group = Group::general,
				Group mask = Group::all,
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
