// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Math;

// QuadtreeIntegral::Node:

QuadtreeIntegral::Node::Node()
{
	VERUS_ZERO_MEM(_ijOffset);
	VERUS_ZERO_MEM(_xzMin);
	VERUS_ZERO_MEM(_xzMax);
}

QuadtreeIntegral::Node::~Node()
{
}

int QuadtreeIntegral::Node::GetChildIndex(int currentNode, int child)
{
	return 1 + (currentNode << 2) + child;
}

bool QuadtreeIntegral::Node::HasChildren(int currentNode, int nodeCount)
{
	return GetChildIndex(currentNode, 3) < nodeCount;
}

void QuadtreeIntegral::Node::PrepareBounds2D()
{
	const Point3 minPoint(float(_xzMin[0]), 0, float(_xzMin[1]));
	const Point3 maxPoint(float(_xzMax[0]), 0, float(_xzMax[1]));
	_bounds.Set(minPoint, maxPoint);
}

void QuadtreeIntegral::Node::PrepareMinMax(int mapHalf)
{
	_xzMin[0] = _xzMin[1] = -mapHalf;
	_xzMax[0] = _xzMax[1] = mapHalf;
}

void QuadtreeIntegral::Node::PrepareOffsetIJ(int mapHalf)
{
	_ijOffset[0] = _xzMin[1] + mapHalf;
	_ijOffset[1] = _xzMin[0] + mapHalf;
}

// QuadtreeIntegral:

QuadtreeIntegral::QuadtreeIntegral()
{
}

QuadtreeIntegral::~QuadtreeIntegral()
{
	Done();
}

void QuadtreeIntegral::Init(int mapSide, int limit, PQuadtreeIntegralDelegate p, float fattenBy)
{
	VERUS_INIT();

	VERUS_RT_ASSERT(Math::IsPowerOfTwo(mapSide));

	_pDelegate = p;

	_limit = limit;
	_mapSide = mapSide;
	_fattenBy = fattenBy;

	AllocNodes();
	InitNodes();
}

void QuadtreeIntegral::Done()
{
	VERUS_DONE(QuadtreeIntegral);
}

void QuadtreeIntegral::AllocNodes()
{
	_maxDepth = Math::HighestBit(_mapSide / _limit);
	const int depthCount = _maxDepth + 1;
	_nodeCount = 0;
	VERUS_FOR(i, depthCount)
		_nodeCount += (1 << i) * (1 << i);
	_vNodes.resize(_nodeCount);
}

void QuadtreeIntegral::InitNodes(int currentNode, int depth)
{
	RNode node = _vNodes[currentNode];
	if (!currentNode)
	{
		node.PrepareMinMax(_mapSide / 2);
	}
	else
	{
		RNode parent = _vNodes[_testCount];

		const int srcMin[2] = { parent.GetSetMin()[0], parent.GetSetMin()[1] };
		const int srcMax[2] = { parent.GetSetMax()[0], parent.GetSetMax()[1] };
		const int* ppSrcMinMax[2] = { srcMin, srcMax };

		int destMin[2];
		int destMax[2];
		int* ppDestMinMax[2] = { destMin, destMax };

		int index = -1;
		VERUS_FOR(i, 4)
		{
			if (Node::GetChildIndex(_testCount, i) == currentNode)
			{
				index = i;
				break;
			}
		}

		Math::Quadrant(ppSrcMinMax, ppDestMinMax, parent.GetWidth() / 2, index);
		node.GetSetMin()[0] = destMin[0];
		node.GetSetMin()[1] = destMin[1];
		node.GetSetMax()[0] = destMax[0];
		node.GetSetMax()[1] = destMax[1];
	}

	node.PrepareOffsetIJ(_mapSide >> 1);
	node.PrepareBounds2D();

	if (depth < _maxDepth)
	{
		VERUS_FOR(i, 4)
		{
			const int childIndex = Node::GetChildIndex(currentNode, i);

			_testCount = currentNode;
			InitNodes(childIndex, depth + 1);

			Bounds bounds = node.GetBounds();
			node.SetBounds(bounds.CombineWith(_vNodes[childIndex].GetBounds()));
		}
	}
	else
	{
		float h[2];
		_pDelegate->QuadtreeIntegral_GetHeights(node.GetOffsetIJ(), h);
		Bounds bounds = node.GetBounds();
		node.SetBounds(bounds.FattenBy(_fattenBy).Set(h[0], h[1] + _fattenBy, 1));
	}

	node.SetSphere(node.GetBounds().GetSphere());
}

void QuadtreeIntegral::TraverseVisible(int currentNode, int depth)
{
	VERUS_QREF_SM;

	if (!currentNode)
	{
		_testCount = 0;
		_passedTestCount = 0;
	}

	_testCount++;

	bool testFrustum = true;
	if (_distCoarseMode && depth + 2 >= _maxDepth)
	{
		RcPoint3 eyePos = sm.GetMainCamera()->GetEyePosition();
		RcPoint3 nodePos = _vNodes[currentNode].GetSphere().GetCenter();
		const float distSq = VMath::distSqr(eyePos, nodePos);
		if (_maxDepth == depth && distSq >= 500 * 500.f)
			testFrustum = false;
		else if (_maxDepth == depth + 1 && distSq >= 1000 * 1000.f)
			testFrustum = false;
		else if (_maxDepth == depth + 2 && distSq >= 1500 * 1500.f && (currentNode & 0x1))
			testFrustum = false;
	}

	RFrustum frustum = sm.GetCamera()->GetFrustum();
	if (testFrustum && Relation::outside == frustum.ContainsAabb(_vNodes[currentNode].GetBounds()))
		return;

	// Yes, it is visible:
	if (Node::HasChildren(currentNode, _nodeCount)) // Node has children -> update them:
	{
		VERUS_FOR(i, 4)
		{
			const int childIndex = Node::GetChildIndex(currentNode, i);
			TraverseVisible(childIndex, depth + 1);
		}
	}
	else // Smallest node -> update it:
	{
		_passedTestCount++;
		_pDelegate->QuadtreeIntegral_ProcessVisibleNode(
			_vNodes[currentNode].GetOffsetIJ(),
			_vNodes[currentNode].GetSphere().GetCenter());
	}
}
