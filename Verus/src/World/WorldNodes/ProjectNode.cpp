// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// ProjectNode:

ProjectNode::ProjectNode()
{
	_type = NodeType::project;
}

ProjectNode::~ProjectNode()
{
	Done();
}

void ProjectNode::Init(RcDesc desc)
{
	if (!(_flags & Flags::readOnlyFlags))
		SetOctreeElementFlag();
	BaseNode::Init(desc._name ? desc._name : "Project");

	_trToBoxSpace = VMath::inverse(GetTransform());

	if (desc._url)
		LoadTexture(desc._url);
}

void ProjectNode::Done()
{
	RemoveTexture();
	VERUS_DONE(ProjectNode);
}

void ProjectNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	RProjectNode projectNode = static_cast<RProjectNode>(node);

	_levels = projectNode._levels;
	_url = projectNode._url;

	if (NodeType::project == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = _C(_url);
		Init(desc);
	}
}

void ProjectNode::Update()
{
	if (!_csh.IsSet() && _tex && _tex->GetTex()->IsLoaded())
	{
		VERUS_QREF_RENDERER;
		_csh = renderer.GetDS().GetProjectNodeShader()->BindDescriptorSetTextures(3, { _tex->GetTex() });
	}
}

void ProjectNode::DrawEditorOverlays(DrawEditorOverlaysFlags flags)
{
	BaseNode::DrawEditorOverlays(flags);

	if (flags & DrawEditorOverlaysFlags::extras)
	{
		VERUS_QREF_EO;

		const Transform3 tr = GetTransform();
		const UINT32 color = EditorOverlays::GetBasisColorY(false, 255);
		eo.DrawBox(&tr, color);
	}
}

void ProjectNode::UpdateBounds()
{
	VERUS_QREF_WM;

	const Math::Bounds bounds(Point3::Replicate(-0.5f), Point3::Replicate(+0.5f));

	if (!IsOctreeBindOnce())
	{
		_bounds = Math::Bounds::MakeFromOrientedBox(bounds, GetTransform());
		if (IsOctreeElement())
		{
			wm.GetOctree().BindElement(Math::Octree::Element(_bounds, this), IsDynamic());
			SetOctreeBindOnceFlag(IsDynamic());
		}
	}
	if (IsDynamic())
	{
		_bounds = Math::Bounds::MakeFromOrientedBox(bounds, GetTransform());
		if (IsOctreeElement())
			wm.GetOctree().UpdateDynamicBounds(Math::Octree::Element(_bounds, this));
	}
}

void ProjectNode::OnNodeTransformed(PBaseNode pNode, bool afterEvent)
{
	BaseNode::OnNodeTransformed(pNode, afterEvent);

	if (afterEvent)
	{
		if (pNode == this)
			_trToBoxSpace = VMath::inverse(GetTransform());
	}
}

void ProjectNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream << _levels.GLM();
	stream.WriteString(_C(_url));
}

void ProjectNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	glm::vec4 levels;
	char url[IO::Stream::s_bufferSize] = {};

	stream >> levels;
	stream.ReadString(url);

	_levels = levels;
	_url = url;

	if (NodeType::project == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = _C(_url);
		Init(desc);
	}
}

void ProjectNode::LoadTexture(CSZ url)
{
	RemoveTexture();

	_url = url;
	_tex.Init(url);
}

void ProjectNode::RemoveTexture()
{
	if (_tex)
	{
		VERUS_QREF_RENDERER;

		renderer->WaitIdle();
		renderer.GetDS().GetProjectNodeShader()->FreeDescriptorSet(_csh);
		_tex.Done();
		_url.clear();
	}
}

// ProjectNodePtr:

void ProjectNodePtr::Init(ProjectNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertProjectNode();
	_p->Init(desc);
}

void ProjectNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertProjectNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void ProjectNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
