#pragma once

namespace verus
{
	namespace Math
	{
		class QuadtreeDelegate
		{
		public:
			virtual Continue Quadtree_ProcessNode(int index, void* pUser) = 0;
		};
		VERUS_TYPEDEFS(QuadtreeDelegate);

		class Quadtree : public Object
		{
		public:
			class Client
			{
			public:
				Bounds _bounds;
				int    _userIndex;

				Client() {}
				Client(RcBounds bounds, int index) :
					_bounds(bounds), _userIndex(index) {}
			};
			VERUS_TYPEDEFS(Client);

		private:
			class Node : public AllocatorAware
			{
				Bounds         _bounds;
				Vector<Client> _vClients;
				int            _children[4];

			public:
				Node();
				~Node();

				RcBounds GetBounds() const { return _bounds; }
				void     SetBounds(RcBounds b) { _bounds = b; }

				int  GetChildIndex(int child) const { return _children[child]; }
				void SetChildIndex(int child, int index) { _children[child] = index; }

				void BindClient(RcClient client);
				void UnbindClient(int index);

				int GetClientCount() const { return Utils::Cast32(_vClients.size()); }
				RcClient GetClientAt(int i) const { return _vClients[i]; }
			};
			VERUS_TYPEDEFS(Node);

			Bounds            _bounds;
			Vector3           _limit = Vector3(0);
			Vector<Node>      _vNodes;
			PQuadtreeDelegate _pDelegate = nullptr;
			int               _nodeCount = 0;

		public:
			class Result
			{
			public:
				int _testCount = 0;
				int _passedTestCount = 0;
				int _lastFoundIndex = -1;
			};
			VERUS_TYPEDEFS(Result);

			Quadtree();
			~Quadtree();

			void Init(RcBounds bounds, RcVector3 limit);
			void Done();

			PQuadtreeDelegate SetDelegate(PQuadtreeDelegate p) { return Utils::Swap(_pDelegate, p); }

			VERUS_P(void Build(int currentNode = 0, int level = 0));

			bool BindClient(RcClient client, bool forceRoot = false, int currentNode = 0);
			void UnbindClient(int index);
			VERUS_P(bool MustBind(int currentNode, RcBounds bounds) const);

			Continue TraverseVisible(RcPoint3 point, PResult pResult = nullptr, int currentNode = 0, void* pUser = nullptr) const;

			VERUS_P(static void ChildIndices(RcPoint3 point, RcPoint3 center, BYTE childIndices[4]));

			RcBounds GetBounds() const { return _bounds; }
		};
		VERUS_TYPEDEFS(Quadtree);
	}
}
