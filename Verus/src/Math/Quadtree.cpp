#include "verus.h"

using namespace verus;
using namespace verus::Math;

// Quadtree::Node:

Quadtree::Node::Node()
{
	VERUS_FOR(i, 4)
		_children[i] = -1;
}

Quadtree::Node::~Node()
{
}

void Quadtree::Node::BindEntity(RcEntity entity)
{
	_vEntities.push_back(entity);
}

void Quadtree::Node::UnbindEntity(int index)
{
	VERUS_WHILE(Vector<Entity>, _vEntities, it)
	{
		if (index == (*it)._userIndex)
			it = _vEntities.erase(it);
		else
			it++;
	}
}

// Quadtree:

Quadtree::Quadtree()
{
}

Quadtree::~Quadtree()
{
	Done();
}

void Quadtree::Init(RcBounds bounds, RcVector3 limit)
{
	VERUS_INIT();

	_bounds = bounds;
	_limit = limit;

	Build();
}

void Quadtree::Done()
{
	VERUS_DONE(Quadtree);
}

void Quadtree::Build(int currentNode, int level)
{
	const Vector3 dim = _bounds.GetDimensions();
	const Vector3 ratio = VMath::divPerElem(dim, _limit);
	const int maxLevelX = Math::HighestBit(int(ratio.getX()));
	const int maxLevelY = Math::HighestBit(int(ratio.getY()));
	const int maxLevel = Math::Max(maxLevelX, maxLevelY);
	if (!currentNode)
	{
		const int levelCount = maxLevel + 1;
		int nodeCount = 0;
		VERUS_FOR(i, levelCount)
		{
			nodeCount +=
				(1 << Math::Min(i, maxLevelX)) *
				(1 << Math::Min(i, maxLevelY));
		}
		_vNodes.resize(nodeCount);
		_vNodes[0].SetBounds(_bounds);
		_nodeCount = 1;
	}

	if (level < maxLevel)
	{
		VERUS_FOR(i, 4)
		{
			RcBounds currentBounds = _vNodes[currentNode].GetBounds();

			Bounds bounds;
			currentBounds.GetQuadrant2D(i, bounds);

			if (level >= maxLevelX)
			{
				bounds.Set(currentBounds, 0);
				if ((i >> 0) & 0x1)
					continue;
			}
			if (level >= maxLevelY)
			{
				bounds.Set(currentBounds, 1);
				if ((i >> 1) & 0x1)
					continue;
			}

			const int index = _nodeCount++;
			_vNodes[currentNode].SetChildIndex(i, index);
			_vNodes[index].SetBounds(bounds);

			Build(index, level + 1);
		}
	}
}

bool Quadtree::BindEntity(RcEntity entity, bool forceRoot, int currentNode)
{
	if (!currentNode)
	{
		if (_vNodes.empty())
			return false; // Quadtree is not ready.
		UnbindEntity(entity._userIndex);
	}

	Entity entityEx = entity;
	if (forceRoot)
		entityEx._bounds = _vNodes[currentNode].GetBounds();

	if (MustBind(currentNode, entityEx._bounds))
	{
		_vNodes[currentNode].BindEntity(entityEx);
		return true;
	}
	else
	{
		VERUS_FOR(i, 4)
		{
			const int index = _vNodes[currentNode].GetChildIndex(i);
			if (index >= 0)
			{
				if (BindEntity(entity, false, index))
					return true;
			}
		}
	}
	return false;
}

void Quadtree::UnbindEntity(int index)
{
	VERUS_FOREACH(Vector<Node>, _vNodes, it)
		(*it).UnbindEntity(index);
}

bool Quadtree::MustBind(int currentNode, RcBounds bounds) const
{
	int count = 0;
	VERUS_FOR(i, 4)
	{
		const int index = _vNodes[currentNode].GetChildIndex(i);
		if (index >= 0)
		{
			if (_vNodes[index].GetBounds().IsOverlappingWith2D(bounds, 2))
				count++;
			if (count > 1)
				return true;
		}
	}
	if (!count)
		return _vNodes[currentNode].GetBounds().IsOverlappingWith2D(bounds, 2);
	return false;
}

Continue Quadtree::TraverseProper(RcPoint3 point, PResult pResult, int currentNode, void* pUser) const
{
	if (_vNodes.empty())
		return Continue::no;

	if (!currentNode)
	{
		pResult->_testCount = 0;
		pResult->_passedTestCount = 0;
		pResult->_lastFoundIndex = -1;
	}

	pResult->_testCount++;

	if (_vNodes[currentNode].GetBounds().IsInside2D(point))
	{
		{
			RcNode node = _vNodes[currentNode];
			const int count = node.GetEntityCount();
			VERUS_FOR(i, count)
			{
				RcEntity entity = node.GetEntityAt(i);
				pResult->_testCount++;
				if (entity._bounds.IsInside2D(point))
				{
					pResult->_lastFoundIndex = entity._userIndex;
					pResult->_passedTestCount++;
					if (Continue::no == _pDelegate->Quadtree_ProcessNode(pResult->_lastFoundIndex, pUser))
						return Continue::no;
				}
			}
		}

		BYTE childIndices[4];
		ChildIndices(point, _vNodes[currentNode].GetBounds().GetCenter(), childIndices);
		VERUS_FOR(i, 4)
		{
			const int index = _vNodes[currentNode].GetChildIndex(childIndices[i]);
			if (index >= 0)
			{
				if (Continue::no == TraverseProper(point, pResult, index, pUser))
					return Continue::no;
			}
		}
	}
	return Continue::yes;
}

void Quadtree::ChildIndices(RcPoint3 point, RcPoint3 center, BYTE childIndices[4])
{
	int i =
		(point.getX() >= center.getX() ? 1 : 0) |
		(point.getY() >= center.getY() ? 2 : 0);
	childIndices[0] = i;
	childIndices[1] = i ^ 0x1;
	i = (~i) & 0x03;
	childIndices[2] = i ^ 0x1;
	childIndices[3] = i;
}
