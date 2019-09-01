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

bool QuadtreeIntegral::Node::HasChildren(int currentNode, int numNodes)
{
	return GetChildIndex(currentNode, 3) < numNodes;
}

void QuadtreeIntegral::Node::PrepareBounds2D()
{
	const Point3 pointMin(float(_xzMin[0]), 0, float(_xzMin[1]));
	const Point3 pointMax(float(_xzMax[0]), 0, float(_xzMax[1]));
	_bounds.Set(pointMin, pointMax);
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
	const int maxDepth = Math::HighestBit(_mapSide / _limit);
	const int numDepths = maxDepth + 1;
	_numNodes = 0;
	VERUS_FOR(i, numDepths)
		_numNodes += (1 << i)*(1 << i);
	_vNodes.resize(_numNodes);
}

void QuadtreeIntegral::InitNodes(int currentNode, int depth)
{
	RNode node = _vNodes[currentNode];
	const int maxDepth = Math::HighestBit(_mapSide / _limit);
	if (!currentNode)
	{
		node.PrepareMinMax(_mapSide / 2);
	}
	else
	{
		RNode parent = _vNodes[_numTests];

		const int srcMin[2] = { parent.GetSetMin()[0], parent.GetSetMin()[1] };
		const int srcMax[2] = { parent.GetSetMax()[0], parent.GetSetMax()[1] };
		const int* ppSrcMinMax[2] = { srcMin, srcMax };

		int destMin[2];
		int destMax[2];
		int* ppDestMinMax[2] = { destMin, destMax };

		int index = -1;
		VERUS_FOR(i, 4)
		{
			if (Node::GetChildIndex(_numTests, i) == currentNode)
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

	if (depth < maxDepth)
	{
		VERUS_FOR(i, 4)
		{
			const int childIndex = Node::GetChildIndex(currentNode, i);

			_numTests = currentNode;
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

void QuadtreeIntegral::TraverseVisible(int currentNode)
{
	if (!currentNode)
	{
		_numTests = 0;
		_numTestsPassed = 0;
	}

	_numTests++;

	{
		Bounds bounds = _vNodes[currentNode].GetBounds();
		Sphere sphere = _vNodes[currentNode].GetSphere();

#if 0
		if (Scene::CWater::I().IsReflectionMode())
		{
			Point3 c = sphere.GetCenter();
			c.setY(-c.getY());
			sphere.SetCenter(c);
			bounds.MirrorY();
		}
#endif

		if (Relation::outside == Scene::SceneManager::I().GetCamera()->GetFrustum().ContainsAabb(bounds))
			return;
	}

	// Yes, it is visible:
	if (Node::HasChildren(currentNode, _numNodes)) // Node has children -> update them:
	{
		VERUS_FOR(i, 4)
		{
			const int childIndex = Node::GetChildIndex(currentNode, i);
			TraverseVisible(childIndex);
		}
	}
	else // Smallest node -> update it:
	{
		_numTestsPassed++;
		_pDelegate->QuadtreeIntegral_ProcessVisibleNode(
			_vNodes[currentNode].GetOffsetIJ(),
			_vNodes[currentNode].GetSphere().GetCenter());
	}
}
