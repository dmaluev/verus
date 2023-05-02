// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Math
	{
		class QuadtreeDelegate
		{
		public:
			virtual Continue Quadtree_OnElementDetected(void* pToken, void* pUser) = 0;
		};
		VERUS_TYPEDEFS(QuadtreeDelegate);

		class Quadtree : public Object
		{
		public:
			class Element
			{
			public:
				Bounds _bounds;
				void* _pToken = nullptr;

				Element() {}
				Element(RcBounds bounds, void* pToken) :
					_bounds(bounds), _pToken(pToken) {}
			};
			VERUS_TYPEDEFS(Element);

			class Result
			{
			public:
				void* _pLastFoundToken = nullptr;
				int   _testCount = 0;
				int   _passedTestCount = 0;
			};
			VERUS_TYPEDEFS(Result);

		private:
			class Node : public AllocatorAware
			{
				Bounds          _bounds;
				Vector<Element> _vElements;

			public:
				Node();
				~Node();

				static int GetChildIndex(int currentNode, int child);
				static bool HasChildren(int currentNode, int nodeCount);

				RcBounds GetBounds() const { return _bounds; }
				void     SetBounds(RcBounds b) { _bounds = b; }

				void BindElement(RcElement element);
				void UnbindElement(void* pToken);
				void UpdateDynamicElement(RcElement element);

				int GetElementCount() const { return Utils::Cast32(_vElements.size()); }
				RcElement GetElementAt(int i) const { return _vElements[i]; }
			};
			VERUS_TYPEDEFS(Node);

			Bounds            _bounds;
			Vector3           _limit = Vector3(0);
			Vector<Node>      _vNodes;
			PQuadtreeDelegate _pDelegate = nullptr;
			Result            _defaultResult;

		public:
			Quadtree();
			~Quadtree();

			void Init(RcBounds bounds, RcVector3 limit);
			void Done();

			PQuadtreeDelegate SetDelegate(PQuadtreeDelegate p) { return Utils::Swap(_pDelegate, p); }

			VERUS_P(void Build(int currentNode = 0, int depth = 0));

			bool BindElement(RcElement element, bool forceRoot = false, int currentNode = 0);
			void UnbindElement(void* pToken);
			void UpdateDynamicBounds(RcElement element);
			VERUS_P(bool MustBind(int currentNode, RcBounds bounds) const);

			Continue DetectElements(RcPoint3 point, PResult pResult = nullptr, int currentNode = 0, void* pUser = nullptr);

			VERUS_P(static void RemapChildIndices(RcPoint3 point, RcPoint3 center, BYTE childIndices[4]));

			RcBounds GetBounds() const { return _bounds; }
		};
		VERUS_TYPEDEFS(Quadtree);
	}
}
