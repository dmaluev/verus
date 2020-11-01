// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Math
	{
		class QuadtreeIntegralDelegate
		{
		public:
			virtual void QuadtreeIntegral_ProcessVisibleNode(const short ij[2], RcPoint3 center) = 0;
			virtual void QuadtreeIntegral_GetHeights(const short ij[2], float height[2]) = 0;
		};
		VERUS_TYPEDEFS(QuadtreeIntegralDelegate);

		class QuadtreeIntegral : public Object
		{
			class Node
			{
				Bounds _bounds;
				Sphere _sphere;
				short  _ijOffset[2];
				short  _xzMin[2];
				short  _xzMax[2];

			public:
				Node();
				~Node();

				static int GetChildIndex(int currentNode, int child);
				static bool HasChildren(int currentNode, int nodeCount);

				const short* GetOffsetIJ() const { return _ijOffset; }
				int GetWidth() const { return _xzMax[0] - _xzMin[0]; }
				short* GetSetMin() { return _xzMin; }
				short* GetSetMax() { return _xzMax; }

				RcBounds GetBounds() const { return _bounds; }
				Node& SetBounds(RcBounds bounds) { _bounds = bounds; return *this; }
				RcSphere GetSphere() const { return _sphere; }
				Node& SetSphere(RcSphere sphere) { _sphere = sphere; return *this; }

				void PrepareBounds2D();
				void PrepareMinMax(int mapHalf);
				void PrepareOffsetIJ(int mapHalf);
			};
			VERUS_TYPEDEFS(Node);

			Vector<Node>              _vNodes;
			PQuadtreeIntegralDelegate _pDelegate = nullptr;
			float                     _fattenBy = 0.5f;
			int                       _nodeCount = 0;
			int                       _testCount = 0;
			int                       _passedTestCount = 0;
			int                       _limit = 0;
			int                       _mapSide = 0;
			int                       _maxDepth = 0;
			bool                      _distCoarseMode = false;

		public:
			QuadtreeIntegral();
			~QuadtreeIntegral();

			void Init(int mapSide, int limit, PQuadtreeIntegralDelegate p, float fattenBy = 0.5f);
			void Done();

			void SetDistCoarseMode(bool b) { _distCoarseMode = b; }

			void SetDelegate(PQuadtreeIntegralDelegate p) { _pDelegate = p; }

			VERUS_P(void AllocNodes());
			VERUS_P(void InitNodes(int currentNode = 0, int depth = 0));

			void TraverseVisible(int currentNode = 0, int depth = 0);

			int GetTestCount()       const { return _testCount; }
			int GetPassedTestCount() const { return _passedTestCount; }
		};
		VERUS_TYPEDEFS(QuadtreeIntegral);
	}
}
