#pragma once

namespace verus
{
	namespace Math
	{
		class OctreeDelegate
		{
		public:
			virtual Continue Octree_ProcessNode(void* pToken, void* pUser) = 0;
		};
		VERUS_TYPEDEFS(OctreeDelegate);

		class Octree : public Object
		{
		public:
			class Entity
			{
			public:
				Bounds _bounds;
				Sphere _sphere;
				void*  _pToken = nullptr;

				Entity() {}
				Entity(RcBounds bounds, void* pToken) :
					_bounds(bounds), _pToken(pToken)
				{
					_sphere = _bounds.GetSphere();
				}
			};
			VERUS_TYPEDEFS(Entity);

		private:
			class Node : public AllocatorAware
			{
				Bounds         _bounds;
				Sphere         _sphere;
				Vector<Entity> _vEntities;

			public:
				Node();
				~Node();

				static int GetChildIndex(int currentNode, int child);
				static bool HasChildren(int currentNode, int numNodes);

				RcSphere GetSphere() const { return _sphere; }
				RcBounds GetBounds() const { return _bounds; }
				void SetBounds(RcBounds b) { _bounds = b; _sphere = b.GetSphere(); }

				void BindEntity(RcEntity entity);
				void UnbindEntity(void* pToken);
				void UpdateDynamicEntity(RcEntity entity);

				int GetNumEntities() const { return Utils::Cast32(_vEntities.size()); }
				RcEntity GetEntityAt(int i) const { return _vEntities[i]; }
			};
			VERUS_TYPEDEFS(Node);

			Bounds          _bounds;
			Vector3         _limit = Vector3(0);
			Vector<Node>    _vNodes;
			POctreeDelegate _pDelegate = nullptr;

		public:
			class Result
			{
			public:
				void* _pLastFoundToken = nullptr;
				int   _numTests = 0;
				int   _numTestsPassed = 0;
				bool  _depth = false;
			};
			VERUS_TYPEDEFS(Result);

			Octree();
			~Octree();

			void Init(RcBounds bounds, RcVector3 limit);
			void Done();

			POctreeDelegate SetDelegate(POctreeDelegate p) { return Utils::Swap(_pDelegate, p); }

			VERUS_P(void Build(int currentNode = 0, int depth = 0));

			bool BindEntity(RcEntity entity, bool forceRoot = false, int currentNode = 0);
			void UnbindEntity(void* pToken);
			void UpdateDynamicBounds(RcEntity entity);
			VERUS_P(bool MustBind(int currentNode, RcBounds bounds) const);

			Continue TraverseProper(RcFrustum frustum, PResult pResult = nullptr, int currentNode = 0, void* pUser = nullptr);
			Continue TraverseProper(RcPoint3 point, PResult pResult = nullptr, int currentNode = 0, void* pUser = nullptr);

			VERUS_P(static void RemapChildIndices(RcPoint3 point, RcPoint3 center, BYTE childIndices[8]));

			RcBounds GetBounds() const { return _bounds; }
		};
		VERUS_TYPEDEFS(Octree);
	}
}
