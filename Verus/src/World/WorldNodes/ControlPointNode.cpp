// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// ControlPointNode:

ControlPointNode::ControlPointNode()
{
	_type = NodeType::controlPoint;
}

ControlPointNode::~ControlPointNode()
{
	Done();
}

void ControlPointNode::Init(RcDesc desc)
{
	_pParent = desc._pPathNode;
	UpdateDepth();
	String name;
	if (!desc._name)
		name = String("Cp") + _C(desc._pPathNode->GetName());
	BaseNode::Init(desc._name ? desc._name : _C(name));
}

void ControlPointNode::Done()
{
	VERUS_DONE(ControlPointNode);
}

void ControlPointNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	RControlPointNode controlPointNode = static_cast<RControlPointNode>(node);

	_smoothSegment = controlPointNode._smoothSegment;

	if (!(hierarchyDuplication & HierarchyDuplication::yes) && !controlPointNode._pNext) // Connect control points during duplication?
	{
		controlPointNode._pNext = this;
		controlPointNode._segmentLength = 0;
		_pPrev = &controlPointNode;
	}

	if (NodeType::controlPoint == _type)
		Init(controlPointNode.GetParent());
}

void ControlPointNode::DrawEditorOverlays(DrawEditorOverlaysFlags flags)
{
	BaseNode::DrawEditorOverlays(flags);

	VERUS_QREF_DD;
	VERUS_QREF_WM;

	if (flags & DrawEditorOverlaysFlags::extras)
	{
		const float r = wm.GetPickingShapeHalfExtent() * 2;
		UINT32 color = 0;
		if (IsIsolatedControlPoint())
			color = EditorOverlays::GetBasisColorX(false, 255);
		else if (IsHeadControlPoint())
			color = ~EditorOverlays::GetBasisColorY(false, 0);
		else
			color = ~EditorOverlays::GetBasisColorX(false, 0);

		dd.Begin(CGI::DebugDraw::Type::lines);
		dd.AddLine(GetPosition() - Vector3(r, 0, 0), GetPosition() + Vector3(r, 0, 0), color);
		dd.AddLine(GetPosition() - Vector3(0, r, 0), GetPosition() + Vector3(0, r, 0), color);
		dd.AddLine(GetPosition() - Vector3(0, 0, r), GetPosition() + Vector3(0, 0, r), color);
		dd.End();
	}

	if (flags & DrawEditorOverlaysFlags::unbounded)
	{
		if (!IsTailControlPoint())
		{
			glm::vec3 pos4[4];
			float len3[3];
			GetSegmentData(pos4, len3);
			const float distToHead = Math::SegmentToPointDistance(pos4[1], pos4[2], wm.GetHeadCamera()->GetEyePosition());
			if (distToHead < 2000)
			{
				dd.Begin(CGI::DebugDraw::Type::lines);
				if (_smoothSegment)
				{
					glm::vec3 tan2[2];
					GetSegmentTangents(pos4, len3, tan2);
					const int steps = Math::Clamp(static_cast<int>(_segmentLength * 100 / distToHead), 2, 64);
					const float stride = 1.f / steps;
					float t = stride;
					glm::vec3 posA = pos4[1], posB;
					VERUS_FOR(i, steps)
					{
						posB = glm::hermite(pos4[1], tan2[0], pos4[2], tan2[1], t);
						dd.AddLine(posA, posB, ~EditorOverlays::GetBasisColorX(false, 0), ~EditorOverlays::GetBasisColorX(false, 255));
						posA = posB;
						t += stride;
					}
				}
				else
				{
					dd.AddLine(pos4[1], pos4[2], ~EditorOverlays::GetBasisColorX(false, 0), ~EditorOverlays::GetBasisColorX(false, 255));
				}
				dd.End();
			}
		}
	}
}

void ControlPointNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	BaseNode::GetEditorCommands(v);

	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::controlPoint_insertControlPoint));
}

bool ControlPointNode::CanAutoSelectParentNode() const
{
	return (_pParent && NodeType::controlPoint == _pParent->GetType()) || BaseNode::CanAutoSelectParentNode();
}

bool ControlPointNode::CanSetParent(PBaseNode pNode) const
{
	if (pNode)
	{
		switch (pNode->GetType())
		{
		case NodeType::controlPoint:
		{
			PControlPointNode pControlPointNode = static_cast<PControlPointNode>(pNode);
			return GetPathNode() == pControlPointNode->GetPathNode();
		}
		break;
		case NodeType::path:
		{
			return GetPathNode() == pNode;
		}
		break;
		}
		return false;
	}
	return false;
}

void ControlPointNode::OnNodeDeleted(PBaseNode pNode, bool afterEvent, bool hierarchy)
{
	BaseNode::OnNodeDeleted(pNode, afterEvent, hierarchy);

	if (!afterEvent)
	{
		if (pNode == _pPrev)
		{
			_pPrev = static_cast<PControlPointNode>(pNode)->GetPreviousControlPoint();
		}
		if (pNode == _pNext)
		{
			_pNext = static_cast<PControlPointNode>(pNode)->GetNextControlPoint();
			UpdateSegmentLength();
		}
	}
}

void ControlPointNode::OnNodeParentChanged(PBaseNode pNode, bool afterEvent)
{
	BaseNode::OnNodeParentChanged(pNode, afterEvent);

	if (afterEvent && pNode == this && pNode->GetParent() && pNode->GetParent()->GetType() == NodeType::controlPoint)
	{
		SetTransform(Transform3::identity(), true);
		UpdateRigidBodyTransform();
	}
}

void ControlPointNode::OnNodeTransformed(PBaseNode pNode, bool afterEvent)
{
	BaseNode::OnNodeTransformed(pNode, afterEvent);

	if (afterEvent)
	{
		if (pNode == this || pNode == _pNext)
			UpdateSegmentLength();
	}
}

void ControlPointNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	VERUS_QREF_WM;

	stream << wm.GetIndexOf(_pPrev, true);
	stream << wm.GetIndexOf(_pNext, true);
	stream << _segmentLength;
	stream << _smoothSegment;
	stream << _markedAsHead;
}

void ControlPointNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	int prevIndex = -1;
	int nextIndex = -1;

	stream >> prevIndex;
	stream >> nextIndex;
	stream >> _segmentLength;
	stream >> _smoothSegment;
	stream >> _markedAsHead;

	_pPrev = reinterpret_cast<PControlPointNode>(static_cast<INT64>(prevIndex));
	_pNext = reinterpret_cast<PControlPointNode>(static_cast<INT64>(nextIndex));

	if (NodeType::controlPoint == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._pPathNode = GetParent();
		Init(desc);
	}
}

void ControlPointNode::OnAllNodesDeserialized()
{
	VERUS_QREF_WM;

	_pPrev = static_cast<PControlPointNode>(wm.GetNodeByIndex(static_cast<int>(reinterpret_cast<INT64>(_pPrev))));
	_pNext = static_cast<PControlPointNode>(wm.GetNodeByIndex(static_cast<int>(reinterpret_cast<INT64>(_pNext))));
}

PBaseNode ControlPointNode::GetPathNode() const
{
	PBaseNode pPathNode = _pParent;
	while (pPathNode)
	{
		if (NodeType::path == pPathNode->GetType())
			return pPathNode;
		pPathNode = pPathNode->GetParent();
	}
	return nullptr;
}

void ControlPointNode::InsertControlPoint(PControlPointNode pTargetNode, PcPoint3 pPos)
{
	if (pTargetNode->_pNext)
		MoveTo(VMath::lerp(0.5f, pTargetNode->GetPosition(), pTargetNode->_pNext->GetPosition()));
	else
		MoveTo(pPos ? *pPos : pTargetNode->GetPosition());

	_pPrev = pTargetNode;
	_pNext = pTargetNode->_pNext;
	UpdateSegmentLength();

	if (pTargetNode->_pNext)
		pTargetNode->_pNext->_pPrev = this;
	pTargetNode->_pNext = this;
	pTargetNode->UpdateSegmentLength();
}

bool ControlPointNode::DisconnectPreviousControlPoint()
{
	if (_pPrev)
	{
		_pPrev->_segmentLength = 0;
		_pPrev->_pNext = nullptr;
		_pPrev = nullptr;
		return true;
	}
	return false;
}

bool ControlPointNode::DisconnectNextControlPoint()
{
	if (_pNext)
	{
		_segmentLength = 0;
		_pNext->_pPrev = nullptr;
		_pNext = nullptr;
		return true;
	}
	return false;
}

bool ControlPointNode::IsHeadControlPoint() const
{
	return (!_pPrev && _pNext) || _markedAsHead;
}

bool ControlPointNode::IsTailControlPoint() const
{
	return _pPrev && !_pNext;
}

bool ControlPointNode::IsIsolatedControlPoint() const
{
	return !_pPrev && !_pNext;
}

void ControlPointNode::UpdateSegmentLength()
{
	_segmentLength = _pNext ? VMath::dist(GetPosition(), _pNext->GetPosition()) : 0.f;
}

void ControlPointNode::GetSegmentData(glm::vec3 pos4[4], float len3[3]) const
{
	pos4[1] = GetPosition().GLM();
	len3[1] = _smoothSegment ? _segmentLength : 0;
	if (_pPrev)
	{
		pos4[0] = _pPrev->GetPosition().GLM();
		len3[0] = _pPrev->IsSmoothSegment() ? _pPrev->GetSegmentLength() : 0;
	}
	else
	{
		pos4[0] = pos4[1];
		len3[0] = len3[1];
	}
	if (_pNext)
	{
		pos4[2] = _pNext->GetPosition().GLM();
		len3[2] = _pNext->IsSmoothSegment() ? _pNext->GetSegmentLength() : 0;
		pos4[3] = _pNext->_pNext ? _pNext->_pNext->GetPosition().GLM() : pos4[2];
	}
	else
	{
		pos4[2] = pos4[1];
		len3[2] = len3[1];
		pos4[3] = pos4[1];
	}
}

void ControlPointNode::GetSegmentTangents(glm::vec3 pos4[4], float len3[3], glm::vec3 tan2[2]) const
{
	const float ratioA = len3[0] / Math::Max(VERUS_FLOAT_THRESHOLD, len3[0] + len3[1]);
	const float ratioB = len3[1] / Math::Max(VERUS_FLOAT_THRESHOLD, len3[1] + len3[2]);
	tan2[0] = glm::mix(pos4[1] - pos4[0], pos4[2] - pos4[1], ratioA) * (VERUS_HERMITE_CIRCLE * 2) * (1 - ratioA);
	tan2[1] = glm::mix(pos4[2] - pos4[1], pos4[3] - pos4[2], ratioB) * (VERUS_HERMITE_CIRCLE * 2) * ratioB;
}

bool ControlPointNode::ComputePositionAt(float distance, RPoint3 pos, PVector3 pDir, bool extrapolate) const
{
	PcControlPointNode pNode = this;
	if (_markedAsHead && distance < 0 && _pPrev) // Looped?
	{
		distance += _pPrev->GetSegmentLength();
		pNode = _pPrev;
	}
	PcControlPointNode pStopNode = pNode;
	bool loop = false;
	float offset = 0;
	while (true)
	{
		const bool minOK = (distance >= offset) ||
			(extrapolate && !pNode->GetPreviousControlPoint());
		const bool maxOK = (distance < (offset + pNode->GetSegmentLength())) ||
			(extrapolate && !pNode->GetNextControlPoint());
		if (minOK && maxOK)
		{
			glm::vec3 pos4[4];
			float len3[3];
			pNode->GetSegmentData(pos4, len3);
			const float segmentLength = pNode->GetNextControlPoint() ? Math::Max(VERUS_FLOAT_THRESHOLD, pNode->GetSegmentLength()) : 1;
			const float t = (distance - offset) / segmentLength;
			if (pNode->IsSmoothSegment() && pNode->GetNextControlPoint() && (t >= 0 && t <= 1))
			{
				glm::vec3 tan2[2];
				pNode->GetSegmentTangents(pos4, len3, tan2);
				pos = glm::hermite(pos4[1], tan2[0], pos4[2], tan2[1], t);
				if (pDir)
				{
					float sign = 1;
					float t2 = t + 0.0001f;
					if (t2 > 1)
					{
						t2 = t - 0.0001f;
						sign = -1;
					}
					const Point3 pos2 = glm::hermite(pos4[1], tan2[0], pos4[2], tan2[1], t2);
					*pDir = VMath::normalize(pos2 - pos) * sign;
				}
			}
			else // For edge cases use lerp:
			{
				pos = glm::mix(pos4[1], pos4[2], t);
				if (pDir)
				{
					if (!pNode->GetNextControlPoint()) // Segment has no direction?
					{
						if (pNode->GetPreviousControlPoint())
							*pDir = glm::normalize(pos4[2] - pNode->GetPreviousControlPoint()->GetPosition().GLM());
						else
							*pDir = glm::vec3(0, 0, 1);
					}
					else
					{
						*pDir = glm::normalize(pos4[2] - pos4[1]);
					}
				}
			}
			return true;
		}
		if (pNode->GetNextControlPoint() && !loop)
		{
			offset += pNode->GetSegmentLength();
			pNode = pNode->GetNextControlPoint();
			loop = (pNode == pStopNode);
		}
		else
		{
			pos = pNode->GetPosition();
			return false;
		}
	}
}

// ControlPointNodePtr:

void ControlPointNodePtr::Init(ControlPointNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertControlPointNode();
	_p->Init(desc);
}

void ControlPointNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertControlPointNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void ControlPointNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
