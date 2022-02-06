// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Math;

// Quadtree::Node:

Quadtree::Node::Node()
{
}

Quadtree::Node::~Node()
{
}

int Quadtree::Node::GetChildIndex(int currentNode, int child)
{
	return 1 + (currentNode << 2) + child;
}

bool Quadtree::Node::HasChildren(int currentNode, int nodeCount)
{
	return GetChildIndex(currentNode, 3) < nodeCount;
}

void Quadtree::Node::BindElement(RcElement element)
{
	_vElements.push_back(element);
}

void Quadtree::Node::UnbindElement(void* pToken)
{
	VERUS_WHILE(Vector<Element>, _vElements, it)
	{
		if (pToken == (*it)._pToken)
			it = _vElements.erase(it);
		else
			it++;
	}
}

void Quadtree::Node::UpdateDynamicElement(RcElement element)
{
	for (auto& x : _vElements)
	{
		if (x._pToken == element._pToken)
		{
			x = element;
			return;
		}
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
	_limit.setZ(1);

	Build();
}

void Quadtree::Done()
{
	VERUS_DONE(Quadtree);
}

void Quadtree::Build(int currentNode, int depth)
{
	const Vector3 dim = _bounds.GetDimensions();
	const Vector3 ratio = VMath::divPerElem(dim, _limit);
	const int maxDepthX = Math::HighestBit(static_cast<int>(ratio.getX()));
	const int maxDepthY = Math::HighestBit(static_cast<int>(ratio.getY()));
	const int maxDepth = Math::Max(maxDepthX, maxDepthY);
	if (!currentNode)
	{
		const int depthCount = maxDepth + 1;
		int nodeCount = 0;
		VERUS_FOR(i, depthCount)
		{
			nodeCount +=
				(1 << Math::Min(i, maxDepthX)) *
				(1 << Math::Min(i, maxDepthY));
		}
		_vNodes.resize(nodeCount);
		_vNodes[0].SetBounds(_bounds);
	}

	if (depth < maxDepth) // Has children?
	{
		VERUS_FOR(i, 4)
		{
			RcBounds currentBounds = _vNodes[currentNode].GetBounds();

			Bounds bounds;
			currentBounds.GetQuadrant2D(i, bounds);

			if (depth >= maxDepthX)
			{
				bounds.Set(currentBounds, 0);
				if ((i >> 0) & 0x1)
					continue;
			}
			if (depth >= maxDepthY)
			{
				bounds.Set(currentBounds, 1);
				if ((i >> 1) & 0x1)
					continue;
			}

			const int childIndex = Node::GetChildIndex(currentNode, i);
			_vNodes[childIndex].SetBounds(bounds);
			Build(childIndex, depth + 1);
		}
	}
}

bool Quadtree::BindElement(RcElement element, bool forceRoot, int currentNode)
{
	if (!currentNode)
	{
		if (_vNodes.empty())
			return false; // Quadtree is not ready.
		UnbindElement(element._pToken);
	}

	Element elementEx = element;
	if (forceRoot)
		elementEx._bounds = _vNodes[currentNode].GetBounds();

	if (MustBind(currentNode, elementEx._bounds))
	{
		_vNodes[currentNode].BindElement(elementEx);
		return true;
	}
	else if (Node::HasChildren(currentNode, Utils::Cast32(_vNodes.size())))
	{
		VERUS_FOR(i, 4)
		{
			const int childIndex = Node::GetChildIndex(currentNode, i);
			if (BindElement(element, false, childIndex))
				return true;
		}
	}
	return false;
}

void Quadtree::UnbindElement(void* pToken)
{
	for (auto& node : _vNodes)
		node.UnbindElement(pToken);
}

void Quadtree::UpdateDynamicBounds(RcElement element)
{
	if (_vNodes.empty())
		return; // Quadtree is not ready.

	_vNodes[0].UpdateDynamicElement(element);
}

bool Quadtree::MustBind(int currentNode, RcBounds bounds) const
{
	if (Node::HasChildren(currentNode, Utils::Cast32(_vNodes.size())))
	{
		int count = 0;
		VERUS_FOR(i, 4)
		{
			const int childIndex = Node::GetChildIndex(currentNode, i);
			if (_vNodes[childIndex].GetBounds().IsOverlappingWith2D(bounds, 2))
				count++;
			if (count > 1)
				return true;
		}
		return false;
	}
	return _vNodes[currentNode].GetBounds().IsOverlappingWith2D(bounds, 2);
}

Continue Quadtree::TraverseVisible(RcPoint3 point, PResult pResult, int currentNode, void* pUser)
{
	if (_vNodes.empty())
		return Continue::no;

	if (!currentNode)
	{
		_defaultResult = Result();
		if (!pResult)
			pResult = &_defaultResult;
		pResult->_testCount = 0;
		pResult->_passedTestCount = 0;
		pResult->_pLastFoundToken = nullptr;
	}

	pResult->_testCount++;
	if (_vNodes[currentNode].GetBounds().IsInside2D(point))
	{
		{
			RcNode node = _vNodes[currentNode];
			const int count = node.GetElementCount();
			VERUS_FOR(i, count)
			{
				RcElement element = node.GetElementAt(i);
				pResult->_testCount++;
				if (element._bounds.IsInside2D(point))
				{
					pResult->_passedTestCount++;
					pResult->_pLastFoundToken = element._pToken;
					if (Continue::no == _pDelegate->Quadtree_ProcessNode(element._pToken, pUser))
						return Continue::no;
				}
			}
		}

		if (Node::HasChildren(currentNode, Utils::Cast32(_vNodes.size())))
		{
			BYTE remapped[4];
			RemapChildIndices(point, _vNodes[currentNode].GetBounds().GetCenter(), remapped);
			VERUS_FOR(i, 4)
			{
				const int childIndex = Node::GetChildIndex(currentNode, remapped[i]);
				if (Continue::no == TraverseVisible(point, pResult, childIndex, pUser))
					return Continue::no;
			}
		}
	}
	return Continue::yes;
}

void Quadtree::RemapChildIndices(RcPoint3 point, RcPoint3 center, BYTE childIndices[4])
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
