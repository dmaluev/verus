// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
			class Client
			{
			public:
				Bounds _bounds;
				Sphere _sphere;
				void* _pToken = nullptr;

				Client() {}
				Client(RcBounds bounds, void* pToken) :
					_bounds(bounds), _pToken(pToken)
				{
					_sphere = _bounds.GetSphere();
				}
			};
			VERUS_TYPEDEFS(Client);

			class Result
			{
			public:
				void* _pLastFoundToken = nullptr;
				int   _testCount = 0;
				int   _passedTestCount = 0;
				bool  _depth = false;
			};
			VERUS_TYPEDEFS(Result);

		private:
			class Node : public AllocatorAware
			{
				Bounds         _bounds;
				Sphere         _sphere;
				Vector<Client> _vClients;

			public:
				Node();
				~Node();

				static int GetChildIndex(int currentNode, int child);
				static bool HasChildren(int currentNode, int nodeCount);

				RcSphere GetSphere() const { return _sphere; }
				RcBounds GetBounds() const { return _bounds; }
				void SetBounds(RcBounds b) { _bounds = b; _sphere = b.GetSphere(); }

				void BindClient(RcClient client);
				void UnbindClient(void* pToken);
				void UpdateDynamicClient(RcClient client);

				int GetClientCount() const { return Utils::Cast32(_vClients.size()); }
				RcClient GetClientAt(int i) const { return _vClients[i]; }
			};
			VERUS_TYPEDEFS(Node);

			Bounds          _bounds;
			Vector3         _limit = Vector3(0);
			Vector<Node>    _vNodes;
			POctreeDelegate _pDelegate = nullptr;
			Result          _defaultResult;

		public:
			Octree();
			~Octree();

			void Init(RcBounds bounds, RcVector3 limit);
			void Done();

			POctreeDelegate SetDelegate(POctreeDelegate p) { return Utils::Swap(_pDelegate, p); }

			VERUS_P(void Build(int currentNode = 0, int depth = 0));

			bool BindClient(RcClient client, bool forceRoot = false, int currentNode = 0);
			void UnbindClient(void* pToken);
			void UpdateDynamicBounds(RcClient client);
			VERUS_P(bool MustBind(int currentNode, RcBounds bounds) const);

			Continue TraverseVisible(RcFrustum frustum, PResult pResult = nullptr, int currentNode = 0, void* pUser = nullptr);
			Continue TraverseVisible(RcPoint3 point, PResult pResult = nullptr, int currentNode = 0, void* pUser = nullptr);

			VERUS_P(static void RemapChildIndices(RcPoint3 point, RcPoint3 center, BYTE childIndices[8]));

			RcBounds GetBounds() const { return _bounds; }
		};
		VERUS_TYPEDEFS(Octree);
	}
}
